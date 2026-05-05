#pragma once

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ujcore/data/AssetInfo.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/ResourceInfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/ResourceContext.h"
#include "ujcore/pipeline/GraphPipeline.h"

namespace ujcore {

class PipelineRunner final {
public:
    using ExecutionStep = GraphPipeline::ExecutionStep;

    const BitmapPool* GetBitmapPool() const {
        return bitmapPool_.get();
    }

    // Invokes PipelineBuilder to build the pipeline from the graph state.
    absl::StatusOr<std::vector<AssetInfo>> RebuildFromState(const GraphState& state);

    // Executes the pipeline and returns the graph outputs.
    absl::StatusOr<std::vector<plstate::GraphRunOutput>> RunPipeline();

    absl::Status AddInputBitmap(const NodeId nodeId, const BitmapInfo& bitmapInfo, uint8_t* data);

    // Gets information about the resources created / used in the graph pipeline.
    absl::StatusOr<std::vector<ResourceInfo>> GetPipelineResources() const;

private:
    GraphPipeline pipeline_;
    std::unique_ptr<BitmapPool> bitmapPool_;
    std::unique_ptr<ResourceContext> resourceContext_;
};

}  // namespace ujcore
