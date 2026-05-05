#include "ujcore/pipeline/PipelineRunner.h"

#include <memory>
#include <utility>

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

// Iterate over the nodes in the pipeline, and assign the resource context.
void InternalAssignResourceCtx(std::map<NodeId, NodeStage>& nodeStages, ResourceContext* resourceCtx) {
    for (auto& [nodeId, stage] : nodeStages) {
        if (stage.ntype == plinfo::NodeInfo::NodeType::FN) {
            PipelineFnNode* fnNode = std::get<std::unique_ptr<PipelineFnNode>>(stage.node).get();
            fnNode->SetResourceContext(resourceCtx);
        }
        else if (stage.ntype == plinfo::NodeInfo::NodeType::IN || stage.ntype == plinfo::NodeInfo::NodeType::OUT) {
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
    return PipelineBuilder::GetAssetInfos(state, pipeline_);
}

absl::StatusOr<std::vector<plstate::GraphRunOutput>> PipelineRunner::RunPipeline() {
    std::vector<plstate::GraphRunOutput> result;

    for (auto& step : pipeline_.execSteps) {
        if (std::holds_alternative<EdgePropagateStep>(step)) {
            const auto& edgeStep = std::get<EdgePropagateStep>(step);
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
            if (edgeStep.dstAttr->data == nullptr) {
                LOG(WARNING) << "Propagating null data for dtype: "
                    << AttributeDataTypeToStr(edgeStep.dstAttr->dtype);
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
                ASSIGN_OR_RETURN(auto runEntry, ioNode.GetRunResult());
                result.push_back(std::move(runEntry));
            }
        }
        else {
            LOG(FATAL) << "Invalid step";
        }
    }
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
