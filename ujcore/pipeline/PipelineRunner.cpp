#include "ujcore/pipeline/PipelineRunner.h"

#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/pipeline/PipelineBuilder.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using NodeRunStep = GraphPipeline::NodeRunStep;
using NodeStage = GraphPipeline::NodeStage;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using GraphIOStep = GraphPipeline::GraphIOStep;
using ManualDataStep = GraphPipeline::ManualDataStep;

// Iterate over the function nodes (PipelineFnNode) in the pipeline, and populate the resource
// context with the bitmap pool.
void InternalAssignBitmapPool(std::map<NodeId, NodeStage>& nodeStages, BitmapPool* bitmapPool) {
    for (auto& [nodeId, stage] : nodeStages) {
        if (stage.ntype == plinfo::NodeInfo::NodeType::FN) {
            PipelineFnNode* fnNode = std::get<std::unique_ptr<PipelineFnNode>>(stage.node).get();
            fnNode->GetResourceContext()->setBitmapPool(bitmapPool);
        }
    }
}

}  // namespace

absl::Status PipelineRunner::BuildFromState(const GraphState& state) {
    PipelineBuilder builder(state, pipeline_);
    RETURN_IF_ERROR(builder.Rebuild());
    bitmapPool_ = CreateNewBitmapPoolFromRegistry();

    InternalAssignBitmapPool(pipeline_.nodeStages, bitmapPool_.get());
    return absl::OkStatus();
}

absl::StatusOr<plstate::GraphRunResult> PipelineRunner::RunPipeline() {
    plstate::GraphRunResult result;
    for (auto& step : pipeline_.execSteps) {
        if (std::holds_alternative<EdgePropagateStep>(step)) {
            const auto& edgeStep = std::get<EdgePropagateStep>(step);
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
            if (edgeStep.dstAttr->data == nullptr) {
                LOG(WARNING) << "Propagating null data for dtype: " << AttributeDataTypeToStr(edgeStep.dstAttr->dtype);
            }
        }
        else if (std::holds_alternative<NodeRunStep>(step)) {
            const NodeRunStep& nodeStep = std::get<NodeRunStep>(step);
            ASSIGN_OR_RETURN(bool runStatus, nodeStep.fnNode->RunFunction());
            if (!runStatus) {
                return absl::InternalError("Node run failed");
            }
        }
        else if (std::holds_alternative<GraphIOStep>(step)) {
            const GraphIOStep& ioStep = std::get<GraphIOStep>(step);
            PipelineIONode& ioNode = *ioStep.ioNode;
            RETURN_IF_ERROR(ioNode.RunAsIO());
            if (ioNode.isOutputStage()) {
                // Collect the output values from the graph output nodes.
                const NodeId nodeId = ioNode.GetNodeId();
                auto encodedOutput = ioNode.GetEncodedOutput();
                if (encodedOutput.ok()) {
                    result.outputs[nodeId] = encodedOutput.value();
                } else {
                    result.outputs[nodeId] = std::nullopt;
                }
            }
        }
        else if (std::holds_alternative<ManualDataStep>(step)) {
            return absl::UnimplementedError("TODO: Implement ManualDataStep in PipelineRunner::RunPipeline()");
        }
        else {
            LOG(FATAL) << "Invalid step";
        }
    }
    return result;
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
