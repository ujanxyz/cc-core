#pragma once

#include <functional>
#include <map>
#include <memory>
#include <variant>
#include <vector>

#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/pipeline/PipelineFnNode.h"
#include "ujcore/pipeline/PipelineIONode.h"

namespace ujcore {

// A graph pipeline is a linearized and execution-ready representation of the execution graph.
// It is generated from the graph state and run with the inputs and produce outputs.
struct GraphPipeline {
    // The following step descriptors define the various types of execution steps in a pipeline.

    // An edge propagate step represents the data propagation along an edge.
    struct EdgePropagateStep {
        EdgeId edgeId;
        AttributeData* srcAttr = nullptr;
        AttributeData* dstAttr = nullptr;

        // TODO: Add a per-edge dirty flag here for incremental stepping.
    };

    struct NodeStage {
        grph::NodeInfo::NodeType ntype = grph::NodeInfo::NodeType::FN;
        std::variant<std::unique_ptr<PipelineFnNode>, std::unique_ptr<PipelineIONode>> node;

        // TODO: Add a per-node dirty flag here for incremental stepping.
    };

    // NodeExecGroup encapsulates a node and its incoming edge propagations into a single
    // execution unit. This allows efficient grouping and reordering by topological order
    // without maintaining a linear execution list.
    struct NodeExecGroup {
        NodeId nodeId;
        // All edge propagation steps that deliver data to this node's input slots.
        std::vector<EdgePropagateStep> incomingTransfers;
        // The node execution step (either function node or IO node).
        std::variant<std::reference_wrapper<PipelineFnNode>, std::reference_wrapper<PipelineIONode>> nodeStep;
    };

    // This structure own the stages, namely PipelineFnNode and PipelineIONode,
    // for all nodes in the graph, keyed by node id.
    std::map<NodeId, NodeStage> nodeStages;

    // Execution groups ordered by topological sort. Each group contains incoming edge
    // transfers + node execution step. Groups are iterated in this order for deterministic
    // execution that respects graph dependencies.
    std::vector<NodeExecGroup> nodeGroups;
};

}  // namespace ujcore
