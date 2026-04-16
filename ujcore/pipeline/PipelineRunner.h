#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include "absl/status/statusor.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"
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

struct ManualDataStep {
    std::optional<const plstate::SlotDataManual>* manual = nullptr;
    AttributeData* dstAttr = nullptr;
};

class PipelineRunner final {
public:
    using Step = std::variant<NodeRunStep, EdgePropagateStep, ManualDataStep>;

    absl::Status BuildFromState(const GraphState& state);

    absl::Status RunPipeline();


private:
    std::map<NodeId, std::unique_ptr<PipelineFnNode>> nodes_;
    std::map<int64_t, std::unique_ptr<AttributeData>> attrs_;
    std::vector<Step> execSteps_;
};

}  // namespace ujcore
