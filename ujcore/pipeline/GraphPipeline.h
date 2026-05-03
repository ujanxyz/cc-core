#pragma once

#include <map>
#include <memory>
#include <variant>
#include <vector>

#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/pipeline/PipelineFnNode.h"
#include "ujcore/pipeline/PipelineIONode.h"

namespace ujcore {

// A graph pipeline is a linearized and execution-ready representation of the execution graph.
// It is generated from the graph state and run with the inputs and produce outputs.
struct GraphPipeline {
    // The following step descriptors define the various types of execution steps in a pipeline.

    // A node step represents the execution of a function node.
    struct NodeRunStep {
        PipelineFnNode* fnNode = nullptr;
    };

    // An edge propagate step represents the data propagation along an edge.
    struct EdgePropagateStep {
        AttributeData* srcAttr = nullptr;
        AttributeData* dstAttr = nullptr;
    };

    struct GraphIOStep {
        PipelineIONode* ioNode = nullptr;
    };

    using ExecutionStep = std::variant<
        NodeRunStep,
        EdgePropagateStep,
        GraphIOStep>;

    struct NodeStage {
        plinfo::NodeInfo::NodeType ntype = plinfo::NodeInfo::NodeType::FN;
        std::variant<std::unique_ptr<PipelineFnNode>, std::unique_ptr<PipelineIONode>> node;
    };

    std::map<NodeId, NodeStage> nodeStages;

    // Linear (order-sensitive) sequence of execution steps.
    std::vector<ExecutionStep> execSteps;
};

}  // namespace ujcore
