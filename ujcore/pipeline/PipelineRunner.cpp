#include "ujcore/pipeline/PipelineRunner.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionReturn.h"
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
        .workuri = fnResult.await->workuri,
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

}  // namespace

absl::StatusOr<std::vector<AssetInfo>> PipelineRunner::RebuildFromState(const GraphState& state) {
    RETURN_IF_ERROR(PipelineBuilder::Rebuild(state, pipeline_));
    bitmapPool_ = CreateNewBitmapPoolFromRegistry();

    resourceContext_ = std::make_unique<ResourceContext>();
    resourceContext_->setBitmapPool(bitmapPool_.get());

    InternalAssignResourceCtx(pipeline_.nodeStages, resourceContext_.get());
    nextStepGroupIndex_ = 0;
    completedOutputs_.clear();
    return PipelineBuilder::GetAssetInfos(state, pipeline_);
}

absl::StatusOr<std::vector<grph::GraphRunOutput>> PipelineRunner::RunPipeline() {
    std::vector<grph::GraphRunOutput> result;

    // Execute each node group in topological order.
    for (const auto& group : pipeline_.nodeGroups) {
        // Execute incoming edge propagation steps.
        for (const auto& edgeStep : group.incomingTransfers) {
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
            if (edgeStep.dstAttr->data == nullptr) {
                LOG(WARNING) << "Propagating null data for dtype: "
                    << AttributeDataTypeToStr(edgeStep.dstAttr->dtype);
            }
        }

        // Execute the node step (either function node or IO node).
        if (std::holds_alternative<std::reference_wrapper<PipelineFnNode>>(group.nodeStep)) {
            PipelineFnNode& fnNode = std::get<std::reference_wrapper<PipelineFnNode>>(group.nodeStep).get();
            const ujfunc::FunctionReturn fnResult = fnNode.RunFunction();
            if (fnResult.IsDone() || fnResult.IsAwait()) {
                // Continue execution for full RunPipeline mode.
            } else if (fnResult.IsNoData()) {
                return absl::FailedPreconditionError("Function returned NO_DATA during full pipeline run");
            } else if (fnResult.IsError()) {
                if (fnResult.error.has_value()) {
                    return *fnResult.error;
                }
                return absl::InternalError("Function returned ERROR without status");
            } else {
                return absl::InternalError("Function returned unknown code");
            }
        }
        else if (std::holds_alternative<std::reference_wrapper<PipelineIONode>>(group.nodeStep)) {
            PipelineIONode& ioNode = std::get<std::reference_wrapper<PipelineIONode>>(group.nodeStep).get();
            RETURN_IF_ERROR(ioNode.RunAsIO());
            if (ioNode.isOutputStage()) {
                // Collect the output values from the graph output nodes.
                ASSIGN_OR_RETURN(auto runEntry, ioNode.GetRunResult());
                result.push_back(std::move(runEntry));
            }
        }
        else {
            LOG(FATAL) << "Invalid node step variant";
        }
    }

    return result;
}

absl::StatusOr<flow::FlowStepResult> PipelineRunner::StepPipeline() {
    flow::FlowStepResult result;

    if (pipeline_.nodeGroups.empty()) {
        result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
        return result;
    }

    if (nextStepGroupIndex_ >= pipeline_.nodeGroups.size()) {
        nextStepGroupIndex_ = 0;
    }

    bool reachedEnd = true;
    for (size_t i = nextStepGroupIndex_; i < pipeline_.nodeGroups.size(); ++i) {
        const auto& group = pipeline_.nodeGroups[i];

        // TODO: Skip propagation when edge dirty-bit indicates no change.
        for (const auto& edgeStep : group.incomingTransfers) {
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
            if (edgeStep.dstAttr->data == nullptr) {
                LOG(WARNING) << "Propagating null data for dtype: "
                    << AttributeDataTypeToStr(edgeStep.dstAttr->dtype)
                    << ", edgeId: " << edgeStep.edgeId.value;
            }
        }

        // TODO: Skip node execution when node dirty-bit indicates no change.
        if (std::holds_alternative<std::reference_wrapper<PipelineFnNode>>(group.nodeStep)) {
            PipelineFnNode& fnNode = std::get<std::reference_wrapper<PipelineFnNode>>(group.nodeStep).get();
            const ujfunc::FunctionReturn fnResult = fnNode.RunFunction();

            if (fnResult.IsDone()) {
                nextStepGroupIndex_ = i + 1;
                continue;
            }
            if (fnResult.IsAwait() || fnResult.IsNoData()) {
                if (fnResult.IsAwait()) {
                    ASSIGN_OR_RETURN(const flow::AwaitEntry awaitEntry, ToFlowAwaitEntry(group.nodeId, fnResult));
                    result.awaitInfos.push_back(std::move(awaitEntry));
                }
                // Resume from the same node on the next call.
                nextStepGroupIndex_ = i;
                reachedEnd = false;
                break;
            }
            if (fnResult.IsError()) {
                result.status = flow::FlowStepResult::StatusEnum::ERROR;
                AppendCompletedOutputs(completedOutputs_, &result);
                return result;
            }

            result.status = flow::FlowStepResult::StatusEnum::ERROR;
            AppendCompletedOutputs(completedOutputs_, &result);
            return result;
        }

        if (std::holds_alternative<std::reference_wrapper<PipelineIONode>>(group.nodeStep)) {
            PipelineIONode& ioNode = std::get<std::reference_wrapper<PipelineIONode>>(group.nodeStep).get();
            const absl::Status ioStatus = ioNode.RunAsIO();
            if (!ioStatus.ok()) {
                result.status = flow::FlowStepResult::StatusEnum::ERROR;
                AppendCompletedOutputs(completedOutputs_, &result);
                return result;
            }
            if (ioNode.isOutputStage()) {
                ASSIGN_OR_RETURN(const grph::GraphRunOutput runEntry, ioNode.GetRunResult());
                completedOutputs_[runEntry.nodeId] = ToFlowGraphOutputEntry(runEntry);
            }
            nextStepGroupIndex_ = i + 1;
            continue;
        }

        result.status = flow::FlowStepResult::StatusEnum::ERROR;
        result.outputs.reserve(completedOutputs_.size());
        for (const auto& [nodeId, output] : completedOutputs_) {
            result.outputs.push_back(output);
        }
        return result;
    }

    if (reachedEnd) {
        result.status = flow::FlowStepResult::StatusEnum::SUCCESS;
        nextStepGroupIndex_ = 0;
    } else {
        result.status = flow::FlowStepResult::StatusEnum::PARTIAL;
    }

    AppendCompletedOutputs(completedOutputs_, &result);
    std::sort(result.outputs.begin(), result.outputs.end(),
        [](const flow::GraphOutputEntry& a, const flow::GraphOutputEntry& b) {
            return a.nodeId.value < b.nodeId.value;
        });

    return result;
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
        BitmapInfo bitmapInfo = {
            .id = std::string(bitmap->id()),
            .backend = "wasm_heap",
            .width = bitmap->width(),
            .height = bitmap->height(),
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
