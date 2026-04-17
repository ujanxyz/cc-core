#include "ujcore/pipeline/PipelineRunner.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {

using ::ujcore::plinfo::SlotInfo;
using ::ujcore::plstate::SlotDataManual;
using ::ujcore::plstate::SlotState;

absl::Status PipelineRunner::BuildFromState(const GraphState& state) {
    auto& registry = FunctionRegistry::GetInstance();
    nodes_.clear();

    for (const auto& [nodeId, node] : state.node_infos) {
        std::unique_ptr<FunctionSpec> fnSpec = registry.GetSpecFromUri(node.fnuri);
        std::unique_ptr<FunctionBase> fnInstance = registry.CreateFromUri(node.fnuri);
        if (fnSpec == nullptr || fnInstance == nullptr) {
            LOG(FATAL) << "Failed to create function instance for uri: " << node.fnuri;
        }
        std::unique_ptr<PipelineFnNode> fnNode = std::make_unique<PipelineFnNode>(nodeId, *fnSpec, std::move(fnInstance));

        for (const FuncParamSpec& param : fnSpec->params) {
            const SlotId slotId = {nodeId, param.name};
            auto slotInfoIter = state.slot_infos.find(slotId);
            auto slotStateIter = state.slot_states.find(slotId);
            if (slotInfoIter == state.slot_infos.end() || slotStateIter == state.slot_states.end()) {
                return absl::NotFoundError(absl::StrCat("Slot info / state not found: ", param.name));
            }
            const SlotInfo& slotInfo = slotInfoIter->second;
            const SlotState& slotState = slotStateIter->second;

            if (slotInfo.access == SlotInfo::AccessEnum::I || slotInfo.access == SlotInfo::AccessEnum::M) {
                if (!slotState.manual.has_value() && slotState.inEdges.empty()) {
                    return absl::InvalidArgumentError(
                        absl::StrCat("Slot has no data source: ", nodeId.value, " : ", param.name));
                }
            }

            // TODO: Create `ManualDataStep` entry from `slotState.manual`

            // TODO: Create a SlotStorage from `plinfo::SlotInfo` and add to the node.
            FuncParamAccess access = FuncParamAccess::kUnknown;
            switch (slotInfo.access) {
                case SlotInfo::AccessEnum::I:
                    access = FuncParamAccess::kInput;
                    break;
                case SlotInfo::AccessEnum::O:
                    access = FuncParamAccess::kOutput;
                    break;
                case SlotInfo::AccessEnum::M:
                    access = FuncParamAccess::kInOut;
                    break;
            }

            SlotStorage storage = {
                .access = access,
                .attribute = AttributeData {},
            };

            if (slotState.manual.has_value()) {
                const SlotDataManual& manual = slotState.manual.value();
                storage.overridden = true;
                storage.attribute.dtype = AttributeDataTypeFromStr(manual.dtype);
                // TODO: Populate storage.attribute.data
            }

            fnNode->slots_[param.name] = std::move(storage);
        }
        nodes_[nodeId] = std::move(fnNode);
    }

    // For each node id, collect the edge steps preceding it.
    std::map<NodeId, std::vector<EdgePropagateStep>> edgeStepsByNode;

    for (const auto& [edgeId, edge] : state.edge_infos) {
        RETURN_IF_NOT_FOUND_IN_MAP(auto& n0, nodes_, edge.node0);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& n1, nodes_, edge.node1);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& slot0, n0->slots_, edge.slot0);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& slot1, n1->slots_, edge.slot1);
        edgeStepsByNode[edge.node1].push_back(EdgePropagateStep {
            .srcAttr = &slot0.attribute,
            .dstAttr = &slot1.attribute,
        });
    }

    std::vector<Step> executionSteps;
    executionSteps.clear();
    executionSteps.reserve(nodes_.size() + state.edge_infos.size());

    int numNodeSteps = 0;
    int numEdgeSteps = 0;

    const size_t numSortedNodes = state.topoSortState.sortOrder.size();
    if (numSortedNodes != nodes_.size()) {
        return absl::InternalError(
            absl::StrCat("Node count mismatch: ", numSortedNodes, " vs ", nodes_.size()));
    }
    for (const NodeId nodeId : state.topoSortState.sortOrder) {
        RETURN_IF_NOT_FOUND_IN_MAP(auto& plNode, nodes_, nodeId);

        // Extract (remove) the edge steps preceding this node.
        std::vector<EdgePropagateStep> edgeSteps;
        auto iter = edgeStepsByNode.find(nodeId);
        if (iter != edgeStepsByNode.end()) {
            edgeSteps = std::move(iter->second);
            edgeStepsByNode.erase(iter);
        }

        // Add the node step preceed by the edge steps (its incoming edges).
        for (auto& entry : edgeSteps) {
            executionSteps.push_back(std::move(entry));
            ++numEdgeSteps;
        }
        executionSteps.push_back(NodeRunStep {
            .node = plNode.get(),
        });
        ++numNodeSteps;
    }

    if (!edgeStepsByNode.empty()) {
        return absl::InternalError("Unprocessed node found");
    }
    execSteps_.swap(executionSteps);
    LOG(INFO) << "Built pipeline, nodes: " << numNodeSteps << ", edges: " << numEdgeSteps;
    return absl::OkStatus();
}

absl::Status PipelineRunner::RunPipeline() {
    for (auto& step : execSteps_) {
        if (std::holds_alternative<EdgePropagateStep>(step)) {
            const auto& edgeStep = std::get<EdgePropagateStep>(step);
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
        }
        else if (std::holds_alternative<NodeRunStep>(step)) {
            const NodeRunStep& nodeStep = std::get<NodeRunStep>(step);
            ASSIGN_OR_RETURN(bool runStatus, nodeStep.node->RunFunction());
            if (!runStatus) {
                return absl::InternalError("Node run failed");
            }
        }
        else {
            LOG(FATAL) << "Invalid step";
        }
    }
    return absl::OkStatus();
}

}  // namespace ujcore
