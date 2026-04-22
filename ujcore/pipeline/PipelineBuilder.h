#pragma once

#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/pipeline/GraphPipeline.h"
#include "ujcore/pipeline/PipelineSlot.h"

namespace ujcore {

// This class is responsible for building a `GraphPipeline` from a `GraphState`.
// It is separate from `PipelineRunner` to keep the building logic separate from
// the execution logic.
// It supports building the whole pipeline from empty (the `Rebuild` method), and
// incremental updates in the events of node and edge additions and deletions to
// the graph.
class PipelineBuilder final {
public:
    explicit PipelineBuilder(const GraphState& graph, GraphPipeline& pipeline);

    // Clear any previously built state, and build the entire pipeline from the
    // current state of the graph (`graph_`).
    absl::Status Rebuild();

private:
    absl::Status HandleFunctionNode(const NodeId nodeId, const plinfo::NodeInfo& nodeInfo);
    absl::Status HandleGraphIONode(const NodeId nodeId, const plinfo::NodeInfo& nodeInfo);

    // Helper methods to get the pipeline slot reference.
    PipelineSlot* LookupPipelineSlot(NodeId nodeId, const std::string& slotName);

    const GraphState& graph_;
    GraphPipeline& pipeline_;
};

}  // namespace ujcore