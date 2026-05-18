#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ujcore/function/ResourceContext.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/graph/AssetInfo.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/ResourceInfo.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/pipeline/FlowTypes.h"
#include "ujcore/pipeline/GraphPipeline.h"

namespace ujcore {

class PipelineRunner final {
public:
    const BitmapPool* GetBitmapPool() const {
        return bitmapPool_.get();
    }

    // Invokes PipelineBuilder to build the pipeline from the graph state.
    absl::StatusOr<std::vector<AssetInfo>> RebuildFromState(const GraphState& state);

    // Runs through the stages of the pipeline from the last stopping point, and returns
    // the combined step status and all completed graph outputs (from this step and previous steps).
    absl::StatusOr<flow::FlowStepResult> StepPipeline();

    absl::StatusOr<flow::FlowStepResult> StepPipelineV2();

    absl::Status AddInputBitmap(const NodeId nodeId, const BitmapInfo& bitmapInfo, uint8_t* data);

    // Gets information about the resources created / used in the graph pipeline.
    absl::StatusOr<std::vector<ResourceInfo>> GetPipelineResources() const;

private:
    // Index of the next node group to execute in StepPipeline().
    size_t nextStepGroupIndex_ = 0;

    // Index of the next node group to execute in StepPipeline().
    std::pair<size_t /* stage index */, size_t /* executor index */> nextStageExecutorIndex_{0, 0};


    // Completed graph output entries accumulated across step calls.
    std::map<NodeId, flow::GraphOutputEntry> completedOutputs_;

    GraphPipeline pipeline_;
    std::unique_ptr<BitmapPool> bitmapPool_;
    std::unique_ptr<ResourceContext> resourceContext_;

    // V2 executor helpers
    absl::Status DoAttrTransfers(GraphPipeline::NodeOpsStage* stage);
    absl::Status HandleEncodeOutput(GraphPipeline::NodeOpsStage* stage);
};

}  // namespace ujcore
