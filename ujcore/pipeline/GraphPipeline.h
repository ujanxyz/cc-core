#pragma once

#include <map>
#include <memory>
#include <vector>

#include "ujcore/graph/IdTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/pipeline/FuncExecutor.h"

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

        std::string debugName;

        // TODO: Add a per-edge dirty flag here for incremental stepping.
    };


    //------------------------------------------------------
    // V2 fields.
    struct NodeOpsStage {
        NodeId nodeId;
        // All attribute propagation steps that deliver data to this node's input slots.
        std::vector<EdgePropagateStep> attrTransfers;
        // The atomic execution units.
        // FN nodes have the main function as the last executor.
        std::vector<std::unique_ptr<FuncExecutor>> executors;
    };

    std::map<NodeId, NodeOpsStage> nodeOpsStages;
    std::vector<NodeOpsStage*> stageSequence;
};

}  // namespace ujcore
