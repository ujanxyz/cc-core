#pragma once

#include "ujcore/graph/GraphState.h"
#include "ujcore/pipeline/GraphPipeline.h"

namespace ujcore {

// This class is responsible for building a `GraphPipeline` from a `GraphState`.
// It is separate from `PipelineRunner` to keep the building logic separate from
// the execution logic.
// It supports building the whole pipeline from empty (the `Rebuild` method), and
// incremental updates in the events of node and edge additions and deletions to
// the graph.
class PipelineBuilder final {
public:
    // Clear any previously built state, and build the entire pipeline from the
    // current state of the graph (`graph_`).
    // TODO: Parallelize node-stage and edge-step construction.
    static absl::Status RebuildV2(const GraphState& graph, GraphPipeline& pipeline);

private:
    PipelineBuilder() = delete;
};

}  // namespace ujcore