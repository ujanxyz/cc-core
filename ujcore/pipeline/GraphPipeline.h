#pragma once

#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "ujcore/data/IdTypes.h"
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

    // TODO: Rename function nodes.
    std::map<NodeId, std::unique_ptr<PipelineFnNode>> nodes;
    std::vector<std::unique_ptr<PipelineIONode>> graphInputs;
    std::vector<std::unique_ptr<PipelineIONode>> graphOutputs;

    std::vector<ExecutionStep> execSteps;
};

}  // namespace ujcore
