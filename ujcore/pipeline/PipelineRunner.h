#pragma once

#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/plstate.h"
#include "ujcore/pipeline/GraphPipeline.h"

namespace ujcore {

class PipelineRunner final {
public:
    using ExecutionStep = GraphPipeline::ExecutionStep;

    absl::Status BuildFromState(const GraphState& state);

    absl::StatusOr<plstate::GraphRunResult> RunPipeline();

private:
    GraphPipeline pipeline_;
    std::unique_ptr<BitmapPool> bitmapPool_;
};

}  // namespace ujcore
