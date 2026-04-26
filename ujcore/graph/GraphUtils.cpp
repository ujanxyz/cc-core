#include "ujcore/graph/GraphUtils.h"

#include "absl/log/log.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"

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

}  // namespace

std::vector<plinfo::NodeInfo> GraphUtils::GetAllNodeInfos(const GraphState& state) {
    return InternalGetMapValues(state.nodeInfos);
}

std::vector<plinfo::EdgeInfo> GraphUtils::GetAllEdgeInfos(const GraphState& state) {
    return InternalGetMapValues(state.edgeInfos);
}

std::vector<plinfo::SlotInfo> GraphUtils::GetAllSlotInfos(const GraphState& state) {
    return InternalGetMapValues(state.slotInfos);
}

absl::StatusOr<std::map<NodeId, plstate::NodeState>> GraphUtils::LookupNodeStates(const GraphState& state, const std::vector<NodeId>& nodeIds) {
    std::map<NodeId, plstate::NodeState> result;
    for (const NodeId& nodeId : nodeIds) {
        auto iter = state.nodeStates.find(nodeId);
        if (iter == state.nodeStates.end()) {
            return absl::NotFoundError("Node state not found for node id: " + std::to_string(nodeId.value));
        }
        result[nodeId] = iter->second;
    }
    return result;
}

absl::StatusOr<std::map<SlotId, plstate::SlotState>> GraphUtils::LookupSlotStates(const GraphState& state, const std::vector<SlotId>& slotIds) {
    std::map<SlotId, plstate::SlotState> result;
    for (const SlotId& slotId : slotIds) {
        auto iter = state.slotStates.find(slotId);
        if (iter == state.slotStates.end()) {
            return absl::NotFoundError("Slot state not found for slot id: " + std::to_string(slotId.parent.value) + ":" + slotId.name);
        }
        result[slotId] = iter->second;
    }
    return result;
}

std::optional<plstate::EncodedData> GraphUtils::GetNodeIoData(const GraphState& state, NodeId nodeId) {
    auto iter = state.nodeStates.find(nodeId);
    if (iter == state.nodeStates.end()) {
        LOG(ERROR) << "Node state not found for node id: " << nodeId.value;
        return std::nullopt;
    }
    const plstate::NodeState& nodeState = iter->second;
    return nodeState.encodedData;    
}

std::optional<plstate::NodeState> GraphUtils::CopyNodeState(const GraphState& state, NodeId nodeId) {
    auto iter = state.nodeStates.find(nodeId);
    if (iter == state.nodeStates.end()) {
        LOG(ERROR) << "Node state not found for node id: " << nodeId.value;
        return std::nullopt;
    }
    return iter->second;
}

std::vector<plinfo::SlotInfo> GraphUtils::CopyAllSlotInfos(const GraphState& state, NodeId nodeId) {
    std::vector<plinfo::SlotInfo> slotInfos;
    auto iter = state.nodeInfos.find(nodeId);
    if (iter == state.nodeInfos.end()) {
        LOG(ERROR) << "Node info not found for node id: " << nodeId.value;
        return slotInfos;
    }
    const plinfo::NodeInfo& nodeInfo = iter->second;

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
    const plinfo::NodeInfo& nodeInfo = iter->second;
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
        const plstate::SlotState& slotState = slotStateIter->second;
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
        const plinfo::EdgeInfo& edgeInfo = edgeIter->second;
        downstreamNodeIds.insert(edgeInfo.node1);
    }

    return downstreamNodeIds;
}

}  // namespace ujcore
