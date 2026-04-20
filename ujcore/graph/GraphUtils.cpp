#include "ujcore/graph/GraphUtils.h"

#include "absl/log/log.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"

namespace ujcore {

std::optional<plstate::NodeState> GraphUtils::CopyNodeState(const GraphState& state, NodeId nodeId) {
    auto iter = state.node_states.find(nodeId);
    if (iter == state.node_states.end()) {
        LOG(ERROR) << "Node state not found for node id: " << nodeId.value;
        return std::nullopt;
    }
    return iter->second;
}

std::vector<plinfo::SlotInfo> GraphUtils::CopyAllSlotInfos(const GraphState& state, NodeId nodeId) {
    std::vector<plinfo::SlotInfo> slotInfos;
    auto iter = state.node_infos.find(nodeId);
    if (iter == state.node_infos.end()) {
        LOG(ERROR) << "Node info not found for node id: " << nodeId.value;
        return slotInfos;
    }
    const plinfo::NodeInfo& nodeInfo = iter->second;

    auto addSlotFn = [&slotInfos, &state, nodeId](const std::vector<std::string>& slotNames)-> void {
        for (const std::string& slotName : slotNames) {
            auto slotInfoIter = state.slot_infos.find(SlotId{.parent=nodeId, .name=slotName});
            if (slotInfoIter == state.slot_infos.end()) {
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
    auto iter = state.node_infos.find(nodeId);
    if (iter == state.node_infos.end()) {
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
        auto slotStateIter = state.slot_states.find(slotId);
        if (slotStateIter == state.slot_states.end()) {
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
        auto edgeIter = state.edge_infos.find(edgeId);
        if (edgeIter == state.edge_infos.end()) {
            LOG(ERROR) << "Edge info not found for edge id: " << edgeId.value;
            continue;
        }
        const plinfo::EdgeInfo& edgeInfo = edgeIter->second;
        downstreamNodeIds.insert(edgeInfo.node1);
    }

    return downstreamNodeIds;
}

}  // namespace ujcore
