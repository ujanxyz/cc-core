#pragma once

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ujcore/graph/AssetInfo.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/ResourceInfo.h"
#include "ujcore/graph/GraphTypes.h"
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
    absl::StatusOr<std::vector<grph::GraphRunOutput>> RunPipeline();

    absl::Status AddInputBitmap(const NodeId nodeId, const BitmapInfo& bitmapInfo, uint8_t* data);

    // Gets information about the resources created / used in the graph pipeline.
    absl::StatusOr<std::vector<ResourceInfo>> GetPipelineResources() const;

private:
    GraphPipeline pipeline_;
    std::unique_ptr<BitmapPool> bitmapPool_;
    std::unique_ptr<ResourceContext> resourceContext_;
};

}  // namespace ujcore
