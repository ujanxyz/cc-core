#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/pipeline/PipelineFnNode.h"

namespace ujcore {

struct NodeRunStep {
    PipelineFnNode* node = nullptr;
};

struct EdgePropagateStep {
    AttributeData* srcAttr = nullptr;
    AttributeData* dstAttr = nullptr;
};

class PipelineRunner final {
public:
    using Step = std::variant<NodeRunStep, EdgePropagateStep>;

    bool BuildFromState(const GraphState& state);

private:
    std::map<NodeId, std::unique_ptr<PipelineFnNode>> nodes_;
    std::map<int64_t, std::unique_ptr<AttributeData>> attrs_;
};

}  // namespace ujcore
