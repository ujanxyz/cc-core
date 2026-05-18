#include "ujcore/pipeline/PipelineRunner.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/EncodedAttr.h"
#include "ujcore/function/FunctionReturn.h"
#include "ujcore/pipeline/FuncExecutor.h"
#include "ujcore/pipeline/GraphPipeline.h"
#include "ujcore/pipeline/PipelineBuilder.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using NodeStage = GraphPipeline::NodeStage;

flow::GraphOutputEntry ToFlowGraphOutputEntry(const grph::GraphRunOutput& output) {
    return flow::GraphOutputEntry {
        .nodeId = output.nodeId,
        .dtype = output.dtype,
        .encodedData = output.encodedData,
    };
}

absl::StatusOr<flow::AwaitEntry> ToFlowAwaitEntry(const NodeId nodeId, const ujfunc::FunctionReturn& fnResult) {
    if (!fnResult.await.has_value()) {
        return absl::InternalError(
            absl::StrCat("Function returned AWAIT without await info for node ", nodeId.value));
    }
    return flow::AwaitEntry {
        .nodeId = nodeId,
        .channel = fnResult.await->channel,
        .workId = fnResult.await->workId,
    };
}

void AppendCompletedOutputs(const std::map<NodeId, flow::GraphOutputEntry>& completedOutputs,
                            flow::FlowStepResult* result) {
    result->outputs.reserve(completedOutputs.size());
    for (const auto& [nodeId, output] : completedOutputs) {
        result->outputs.push_back(output);
    }
}

// Iterate over the nodes in the pipeline, and assign the resource context.
void InternalAssignResourceCtx(std::map<NodeId, NodeStage>& nodeStages, ResourceContext* resourceCtx) {
    for (auto& [nodeId, stage] : nodeStages) {
        if (stage.ntype == grph::NodeInfo::NodeType::FN) {
            PipelineFnNode* fnNode = std::get<std::unique_ptr<PipelineFnNode>>(stage.node).get();
            fnNode->SetResourceContext(resourceCtx);
        }
        else if (stage.ntype == grph::NodeInfo::NodeType::IN || stage.ntype == grph::NodeInfo::NodeType::OUT) {
            PipelineIONode* ioNode = std::get<std::unique_ptr<PipelineIONode>>(stage.node).get();
            ioNode->SetResourceContext(resourceCtx);
        }
    }
}

// Helper methods to get the pipeline slot reference.
FuncExecutor::PerSlotEntry* LookupExecutorSlot(GraphPipeline& pipeline_, NodeId nodeId, const std::string& slotName) {
    auto stageIter = pipeline_.nodeOpsStages.find(nodeId);
    if (stageIter == pipeline_.nodeOpsStages.end()) {
        LOG(INFO) << "NodeOpsStage not found for nodeId: " << nodeId.value;
        return nullptr;
    }
    GraphPipeline::NodeOpsStage& nodeOpsStage = stageIter->second;
    const auto& lastExecutor = nodeOpsStage.executors.back();
    auto slotIter = lastExecutor->slotEntries_.find(slotName);
    if (slotIter == lastExecutor->slotEntries_.end()) {
        LOG(INFO) << "Slot entry not found in executor for slotName: " << slotName;
        return nullptr;
    }
    FuncExecutor::PerSlotEntry* slot = &slotIter->second;
    return slot;
}
    
void InternalSetResourceCtx(std::map<NodeId, GraphPipeline::NodeOpsStage>& nodeOpsStages, ResourceContext* resourceContext) {
    for (auto& [nodeId, nodeOps] : nodeOpsStages) {
        for (auto& executor : nodeOps.executors) {
            executor->SetResourceContext(resourceContext);
        }
    }
}

}  // namespace

absl::StatusOr<std::vector<AssetInfo>> PipelineRunner::RebuildFromState(const GraphState& state) {
    RETURN_IF_ERROR(PipelineBuilder::RebuildV2(state, pipeline_));
    // RETURN_IF_ERROR(PipelineBuilder::Rebuild(state, pipeline_));
    bitmapPool_ = CreateNewBitmapPoolFromRegistry();

    resourceContext_ = std::make_unique<ResourceContext>();
    resourceContext_->setBitmapPool(bitmapPool_.get());

    InternalAssignResourceCtx(pipeline_.nodeStages, resourceContext_.get());
    InternalSetResourceCtx(pipeline_.nodeOpsStages, resourceContext_.get());

    nextStepGroupIndex_ = 0;
    nextStageExecutorIndex_ = {0, 0};
    completedOutputs_.clear();
    return PipelineBuilder::GetAssetInfos(state, pipeline_);
}

[[deprecated("Use StepPipelineV2 instead")]]
absl::StatusOr<flow::FlowStepResult> PipelineRunner::StepPipeline() {
    LOG(FATAL) << "StepPipeline is deprecated, please use StepPipelineV2 instead";
    return flow::FlowStepResult {};

    // if (pipeline_.nodeGroups.empty()) {
    //     result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
    //     return result;
    // }

    // if (nextStepGroupIndex_ >= pipeline_.nodeGroups.size()) {
    //     nextStepGroupIndex_ = 0;
    // }

    // bool reachedEnd = true;
    // for (size_t i = nextStepGroupIndex_; i < pipeline_.nodeGroups.size(); ++i) {
    //     const auto& group = pipeline_.nodeGroups[i];

    //     // TODO: Skip propagation when edge dirty-bit indicates no change.
    //     for (const auto& edgeStep : group.incomingTransfers) {
    //         edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
    //         edgeStep.dstAttr->data = edgeStep.srcAttr->data;
    //         if (edgeStep.dstAttr->data == nullptr) {
    //             LOG(WARNING) << "Propagating null data for dtype: "
    //                 << AttributeDataTypeToStr(edgeStep.dstAttr->dtype)
    //                 << ", edgeId: " << edgeStep.edgeId.value;
    //         }
    //     }

    //     // TODO: Skip node execution when node dirty-bit indicates no change.
    //     if (std::holds_alternative<std::reference_wrapper<PipelineFnNode>>(group.nodeStep)) {
    //         PipelineFnNode& fnNode = std::get<std::reference_wrapper<PipelineFnNode>>(group.nodeStep).get();
    //         const ujfunc::FunctionReturn fnResult = fnNode.RunFunction();

    //         if (fnResult.IsDone()) {
    //             nextStepGroupIndex_ = i + 1;
    //             continue;
    //         }
    //         if (fnResult.IsAwait() || fnResult.IsNoData()) {
    //             if (fnResult.IsAwait()) {
    //                 ASSIGN_OR_RETURN(const flow::AwaitEntry awaitEntry, ToFlowAwaitEntry(group.nodeId, fnResult));
    //                 result.awaitInfos.push_back(std::move(awaitEntry));
    //             }
    //             // Resume from the same node on the next call.
    //             nextStepGroupIndex_ = i;
    //             reachedEnd = false;
    //             break;
    //         }
    //         if (fnResult.IsError()) {
    //             result.status = flow::FlowStepResult::StatusEnum::ERROR;
    //             AppendCompletedOutputs(completedOutputs_, &result);
    //             return result;
    //         }

    //         result.status = flow::FlowStepResult::StatusEnum::ERROR;
    //         AppendCompletedOutputs(completedOutputs_, &result);
    //         return result;
    //     }

    //     if (std::holds_alternative<std::reference_wrapper<PipelineIONode>>(group.nodeStep)) {
    //         PipelineIONode& ioNode = std::get<std::reference_wrapper<PipelineIONode>>(group.nodeStep).get();
    //         const absl::Status ioStatus = ioNode.RunAsIO();
    //         if (!ioStatus.ok()) {
    //             result.status = flow::FlowStepResult::StatusEnum::ERROR;
    //             AppendCompletedOutputs(completedOutputs_, &result);
    //             return result;
    //         }
    //         if (ioNode.isOutputStage()) {
    //             ASSIGN_OR_RETURN(const grph::GraphRunOutput runEntry, ioNode.GetRunResult());
    //             completedOutputs_[runEntry.nodeId] = ToFlowGraphOutputEntry(runEntry);
    //         }
    //         nextStepGroupIndex_ = i + 1;
    //         continue;
    //     }

    //     result.status = flow::FlowStepResult::StatusEnum::ERROR;
    //     result.outputs.reserve(completedOutputs_.size());
    //     for (const auto& [nodeId, output] : completedOutputs_) {
    //         result.outputs.push_back(output);
    //     }
    //     return result;
    // }

    // if (reachedEnd) {
    //     result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
    //     nextStepGroupIndex_ = 0;
    // } else {
    //     result.status = flow::FlowStepResult::StatusEnum::PARTIAL;
    // }

    // AppendCompletedOutputs(completedOutputs_, &result);
    // std::sort(result.outputs.begin(), result.outputs.end(),
    //     [](const flow::GraphOutputEntry& a, const flow::GraphOutputEntry& b) {
    //         return a.nodeId.value < b.nodeId.value;
    //     });
}

absl::StatusOr<flow::FlowStepResult> PipelineRunner::StepPipelineV2() {
    using NodeOpsStage = GraphPipeline::NodeOpsStage;

    size_t stageIndex = nextStageExecutorIndex_.first;
    size_t executorIndex = nextStageExecutorIndex_.second;

    flow::FlowStepResult result;
    if (stageIndex >= pipeline_.stageSequence.size()) {
        result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
        return result;
    }

    LOG(INFO) << "Stepping started at node: " << pipeline_.stageSequence[stageIndex]->nodeId.value
              << ", executor index: " << executorIndex;
    bool reachedEnd = false;
    while (!reachedEnd) {
        NodeOpsStage* const pStage = pipeline_.stageSequence[stageIndex];
        LOG(INFO) << "@ nodeId: " << pStage->nodeId.value << ", executorIndex: " << executorIndex
                  << ", numExecutorsInStage: " << pStage->executors.size();
        CHECK(pStage != nullptr);
        const NodeId nodeId = pStage->nodeId;

        if (executorIndex < pStage->executors.size()) {
            if (executorIndex == pStage->executors.size() - 1) {
                // This is the main executor of the stage, run attribute transfers before it.
                RETURN_IF_ERROR(DoAttrTransfers(pStage));
            }
            auto& executor = pStage->executors[executorIndex];
            const ujfunc::FunctionReturn executorRes = executor->RunFunction();

            if (executorRes.IsDone()) {
                // Executor finished, move to the next executor in the same stage.
                if (executor->type_ ==  FuncExecutor::ExecutorType::kEncode) {
                    RETURN_IF_ERROR(HandleEncodeOutput(pStage));
                }
            } else if (executorRes.IsAwait() || executorRes.IsNoData()) {
                if (executorRes.IsAwait()) {
                    LOG(INFO) << "Fn returned => await, nodeId: " << nodeId.value << ", executor debugName: " << executor->debugName_;
                    ASSIGN_OR_RETURN(const flow::AwaitEntry awaitEntry, ToFlowAwaitEntry(nodeId, executorRes));
                    result.awaitInfos.push_back(std::move(awaitEntry));
                }
                break;
            } else if (executorRes.IsError()) {
                LOG(INFO) << "Fn returned => error for nodeId: " << nodeId.value << ", executor debugName: " << executor->debugName_;
                result.status = flow::FlowStepResult::StatusEnum::ERROR;
                AppendCompletedOutputs(completedOutputs_, &result);
                return result;
            } else {
                LOG(FATAL) << "Unexpected FunctionReturn type from executor for nodeId: " << nodeId.value;
            }
        }

        if ((++executorIndex) >= pStage->executors.size()) {
            // Move to the next stage.
            executorIndex = 0;
            if ((++stageIndex) >= pipeline_.stageSequence.size()) {
                stageIndex = 0;
                reachedEnd = true;
                break;
            }
        }
    }

    nextStageExecutorIndex_ = {stageIndex, executorIndex};
    if (reachedEnd) {
        result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
    } else {
        result.status = flow::FlowStepResult::StatusEnum::PARTIAL;
    }
    AppendCompletedOutputs(completedOutputs_, &result);
    return result;
}

absl::Status PipelineRunner::DoAttrTransfers(GraphPipeline::NodeOpsStage* stage) {
    CHECK(stage != nullptr);
    for (const auto& attrTransfer : stage->attrTransfers) {
        LOG(INFO) << "Doing attr transfer for edgeId: " << attrTransfer.edgeId.value << ", debugName: " << attrTransfer.debugName;
        attrTransfer.dstAttr->dtype = attrTransfer.srcAttr->dtype;
        attrTransfer.dstAttr->data = attrTransfer.srcAttr->data;
        if (attrTransfer.dstAttr->data == nullptr) {
            LOG(WARNING) << "Propagating null data for dtype: "
                << AttributeDataTypeToStr(attrTransfer.dstAttr->dtype)
                << ", edgeId: " << attrTransfer.edgeId.value
                << ", debugName: " << attrTransfer.debugName;
        }
    }
    return absl::OkStatus();
}

absl::Status PipelineRunner::HandleEncodeOutput(GraphPipeline::NodeOpsStage* stage) {
    LOG(INFO) << "Processing encode executor for nodeId: " << stage->nodeId.value;
    FuncExecutor::PerSlotEntry* outSlot = LookupExecutorSlot(pipeline_, stage->nodeId, "$out");
    if (outSlot == nullptr) {
        LOG(ERROR) << "Output slot not found for encode executor at nodeId: " << stage->nodeId.value;
        return absl::InternalError(absl::StrCat("Output slot not found for encode executor at nodeId: ", stage->nodeId.value));
    }
    if (outSlot->attribute.dtype != AttributeDataType::kEncoded) {
        LOG(ERROR) << "Unexpected output dtype from encode executor at nodeId: " << stage->nodeId.value
            << ", dtype: " << AttributeDataTypeToStr(outSlot->attribute.dtype);
        return absl::InternalError(absl::StrCat("Unexpected output dtype from encode executor at nodeId: ", stage->nodeId.value, ", dtype: ", AttributeDataTypeToStr(outSlot->attribute.dtype)));
    }
    if (outSlot->attribute.data == nullptr) {
        LOG(WARNING) << "Encode executor produced null data for nodeId: " << stage->nodeId.value;
    }
    auto encodedStorage = std::static_pointer_cast<EncodedAttr::Storage>(outSlot->attribute.data);
    completedOutputs_[stage->nodeId] = flow::GraphOutputEntry {
        .nodeId = stage->nodeId,
        .dtype = AttributeDataTypeToStr(outSlot->dtype),
        .encodedData = grph::EncodedData {
            .payload = encodedStorage->encodedData,
        },
    };
    return absl::OkStatus();
}


absl::Status PipelineRunner::AddInputBitmap(const NodeId nodeId, const BitmapInfo& bitmapInfo, uint8_t* data) {
    if (bitmapPool_ == nullptr) {
        return absl::InternalError("Bitmap pool is not initialized");
    }
    // return bitmapPool_->AddInputBitmap(nodeId, bitmapInfo, data);
    return absl::OkStatus();
}

absl::StatusOr<std::vector<ResourceInfo>> PipelineRunner::GetPipelineResources() const {
    std::vector<ResourceInfo> resources;
    if (bitmapPool_ == nullptr) {
        return resources;
    }
    const std::vector<const Bitmap*> activeBitmaps = bitmapPool_->GetActiveBitmaps();
    for (const Bitmap* bitmap : activeBitmaps) {
        const IDimension dim = bitmap->dimension();
        BitmapInfo bitmapInfo = {
            .id = std::string(bitmap->id()),
            .backend = "wasm_heap",
            .width = dim.width,
            .height = dim.height,
            .bytesPerPixel = bitmap->bytesPerPixel(),
        };
        ResourceInfo info;
        info.type = ResourceInfo::Type::BITMAP;
        info.bitmap = std::move(bitmapInfo);
        resources.push_back(info);
    }
    return resources;
}


}  // namespace ujcore
