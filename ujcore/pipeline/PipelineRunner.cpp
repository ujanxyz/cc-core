#include "ujcore/pipeline/PipelineRunner.h"

#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/pipeline/PipelineBuilder.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using NodeStage = GraphPipeline::NodeStage;

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
            RETURN_IF_ERROR(fnNode.RunFunction());
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
