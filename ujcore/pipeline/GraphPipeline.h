#pragma once

#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/pipeline/PipelineFnNode.h"
#include "ujcore/pipeline/PipelineIONode.h"

namespace ujcore {

struct GraphPipeline {
    struct NodeRunStep {
        PipelineFnNode* fnNode = nullptr;
    };

    struct EdgePropagateStep {
        AttributeData* srcAttr = nullptr;
        AttributeData* dstAttr = nullptr;
    };

    struct ManualDataStep {
        std::optional<const plstate::EncodedData>* manual = nullptr;
        AttributeData* dstAttr = nullptr;
    };

    struct GraphIOStep {
        PipelineIONode* ioNode = nullptr;
    };

    using ExecutionStep = std::variant<
        NodeRunStep,
        EdgePropagateStep,
        ManualDataStep,
        GraphIOStep>;

    struct NodeStage {
        plinfo::NodeInfo::NodeType ntype = plinfo::NodeInfo::NodeType::FN;
        std::variant<std::unique_ptr<PipelineFnNode>, std::unique_ptr<PipelineIONode>> node;
    };

    std::map<NodeId, NodeStage> nodeStages;
    std::vector<ExecutionStep> execSteps;
};

}  // namespace ujcore
