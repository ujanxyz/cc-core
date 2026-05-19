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

absl::StatusOr<int /* dummy return value */> PipelineRunner::RebuildFromState(const GraphState& state) {
    RETURN_IF_ERROR(PipelineBuilder::RebuildV2(state, pipeline_));
    bitmapPool_ = CreateNewBitmapPoolFromRegistry();

    resourceContext_ = std::make_unique<ResourceContext>();
    resourceContext_->setBitmapPool(bitmapPool_.get());

    InternalSetResourceCtx(pipeline_.nodeOpsStages, resourceContext_.get());

    nextStageExecutorIndex_ = {0, 0};
    completedOutputs_.clear();

    return 0;
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

}  // namespace ujcore
