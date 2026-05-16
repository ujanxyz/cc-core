#include "ujcore/graph/GraphUtils.h"

#include "absl/log/log.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/IdTypes.h"

namespace ujcore {
namespace {

template<class K, class T>
std::vector<T> InternalGetMapValues(const std::map<K, T>& infosMap) {
  std::vector<T> result;
  result.reserve(infosMap.size());
  for (const auto& [_, info] : infosMap) {

    result.push_back(info);
  }
  return result;
}

// Template function to handle both const and mutable I/O slot lookups.
// When GraphStateType is 'const GraphState', returns const pointers.
// When GraphStateType is 'GraphState', returns mutable pointers to SlotState.
//
// TODO: Split this into 2 apis: InternalLookupInputSlot and InternalLookupOutputSlot,
// to avoid confusions on the slot name ("$in" vs "$out") and the node type (graph
// input node vs graph output node).
template<typename GraphStateType>
auto InternalLookupIoSlot(GraphStateType& state, NodeId nodeId) 
    -> absl::StatusOr<std::pair<const grph::SlotInfo*, 
                                std::conditional_t<std::is_const_v<GraphStateType>, 
                                                   const grph::SlotState*, 
                                                   grph::SlotState*>>> {
    // Find the node info
    auto nodeInfoIt = state.nodeInfos.find(nodeId);
    if (nodeInfoIt == state.nodeInfos.end()) {
        return absl::NotFoundError("Node info not found for node id: " + std::to_string(nodeId.value));
    }
    const grph::NodeInfo& nodeInfo = nodeInfoIt->second;

    // Determine slot name for I/O node
    std::string slotName;
    if (nodeInfo.ntype == grph::NodeInfo::NodeType::IN) {
        if (nodeInfo.outs.size() != 1) {
            return absl::InvalidArgumentError("Graph input node must have exactly one output slot");
        }
        slotName = nodeInfo.outs[0]; // Should be "$out"
    } else if (nodeInfo.ntype == grph::NodeInfo::NodeType::OUT) {
        if (nodeInfo.ins.size() != 1) {
            return absl::InvalidArgumentError("Graph output node must have exactly one input slot");
        }
        slotName = nodeInfo.ins[0]; // Should be "$in"
    } else {
        return absl::InvalidArgumentError("LookupIoSlot only valid for graph input/output nodes");
    }

    SlotId slotId{nodeId, slotName};
    auto slotInfoIt = state.slotInfos.find(slotId);
    if (slotInfoIt == state.slotInfos.end()) {
        return absl::NotFoundError("Slot info not found for slot id: " + slotName);
    }
    auto slotStateIt = state.slotStates.find(slotId);
    if (slotStateIt == state.slotStates.end()) {
        return absl::NotFoundError("Slot state not found for slot id: " + slotName);
    }
    return std::make_pair(&slotInfoIt->second, &slotStateIt->second);
}

}  // namespace

std::vector<grph::NodeInfo> GraphUtils::GetAllNodeInfos(const GraphState& state) {
    return InternalGetMapValues(state.nodeInfos);
}

std::vector<grph::EdgeInfo> GraphUtils::GetAllEdgeInfos(const GraphState& state) {
    return InternalGetMapValues(state.edgeInfos);
}

std::vector<grph::SlotInfo> GraphUtils::GetAllSlotInfos(const GraphState& state) {
    return InternalGetMapValues(state.slotInfos);
}

absl::StatusOr<std::map<NodeId, grph::NodeState>> GraphUtils::LookupNodeStates(const GraphState& state, const std::vector<NodeId>& nodeIds) {
    std::map<NodeId, grph::NodeState> result;
    for (const NodeId& nodeId : nodeIds) {
        auto iter = state.nodeStates.find(nodeId);
        if (iter == state.nodeStates.end()) {
            return absl::NotFoundError("Node state not found for node id: " + std::to_string(nodeId.value));
        }
        result[nodeId] = iter->second;
    }
    return result;
}

absl::StatusOr<std::map<SlotId, grph::SlotState>> GraphUtils::LookupSlotStates(const GraphState& state, const std::vector<SlotId>& slotIds) {
    std::map<SlotId, grph::SlotState> result;
    for (const SlotId& slotId : slotIds) {
        auto iter = state.slotStates.find(slotId);

        if (iter == state.slotStates.end()) {
            return absl::NotFoundError("Slot state not found for slot id: " + std::to_string(slotId.parent.value) + ":" + slotId.name);
        }
        result[slotId] = iter->second;
    }
    return result;
}

absl::StatusOr<std::pair<const grph::SlotInfo*, const grph::SlotState*>> GraphUtils::LookupIoSlot(const GraphState& state, NodeId nodeId) {
    return InternalLookupIoSlot(state, nodeId);
}

absl::StatusOr<std::pair<const grph::SlotInfo*, grph::SlotState*>> GraphUtils::LookupMutableIoSlot(GraphState& state, NodeId nodeId) {
    return InternalLookupIoSlot(state, nodeId);
}

std::optional<grph::NodeState> GraphUtils::CopyNodeState(const GraphState& state, NodeId nodeId) {
    auto iter = state.nodeStates.find(nodeId);
    if (iter == state.nodeStates.end()) {
        LOG(ERROR) << "Node state not found for node id: " << nodeId.value;
        return std::nullopt;
    }
    return iter->second;
}

std::vector<grph::SlotInfo> GraphUtils::CopyAllSlotInfos(const GraphState& state, NodeId nodeId) {
    std::vector<grph::SlotInfo> slotInfos;
    auto iter = state.nodeInfos.find(nodeId);
    if (iter == state.nodeInfos.end()) {
        LOG(ERROR) << "Node info not found for node id: " << nodeId.value;
        return slotInfos;
    }
    const grph::NodeInfo& nodeInfo = iter->second;

    auto addSlotFn = [&slotInfos, &state, nodeId](const std::vector<std::string>& slotNames)-> void {
        for (const std::string& slotName : slotNames) {
            auto slotInfoIter = state.slotInfos.find(SlotId{.parent=nodeId, .name=slotName});
            if (slotInfoIter == state.slotInfos.end()) {
                LOG(ERROR) << "Slot info not found for slot: " << slotName << " of node: " << nodeId.value;
                continue;
            }
            slotInfos.push_back(slotInfoIter->second);
        }
    };

    addSlotFn(nodeInfo.ins);
    addSlotFn(nodeInfo.outs);
    addSlotFn(nodeInfo.inouts);
    return slotInfos;
}

std::set<NodeId> GraphUtils::GetDownstreamNodeIds(const GraphState& state, NodeId nodeId) {
    // Get the node info, and from that the out slots.
    auto iter = state.nodeInfos.find(nodeId);
    if (iter == state.nodeInfos.end()) {
        LOG(ERROR) << "Node id not found: " << nodeId.value;
        return {};
    }
    const grph::NodeInfo& nodeInfo = iter->second;
    std::set<SlotId> outSlotIds;
    for (const std::string& slot : nodeInfo.outs) {
        outSlotIds.insert(SlotId{.parent=nodeId, .name=slot});
    }
    for (const std::string& slot : nodeInfo.inouts) {
        outSlotIds.insert(SlotId{.parent=nodeId, .name=slot});
    }

    // Collect those slot states, and get the edges from there.
    std::vector<EdgeId> downstreamEdgeIds;
    for (const SlotId& slotId : outSlotIds) {
        auto slotStateIter = state.slotStates.find(slotId);
        if (slotStateIter == state.slotStates.end()) {
            LOG(ERROR) << "Slot state not found for slot: " << slotId.name << " of node: " << nodeId.value;
            continue;
        }
        const grph::SlotState& slotState = slotStateIter->second;
        for (const EdgeId& edgeId : slotState.outEdges) {
            downstreamEdgeIds.push_back(edgeId);
        }
    }

    std::set<NodeId> downstreamNodeIds;
    for (const EdgeId& edgeId : downstreamEdgeIds) {
        auto edgeIter = state.edgeInfos.find(edgeId);
        if (edgeIter == state.edgeInfos.end()) {
            LOG(ERROR) << "Edge info not found for edge id: " << edgeId.value;
            continue;
        }
        const grph::EdgeInfo& edgeInfo = edgeIter->second;
        downstreamNodeIds.insert(edgeInfo.node1);
    }

    return downstreamNodeIds;
}

}  // namespace ujcore
