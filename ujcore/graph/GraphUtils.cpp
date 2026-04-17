#include "ujcore/graph/GraphUtils.h"

#include "absl/log/log.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"

namespace ujcore {

std::optional<plstate::NodeState> GetNodeState(const GraphState& state, NodeId nodeId) {
    auto iter = state.node_states.find(nodeId);
    if (iter == state.node_states.end()) {
        LOG(ERROR) << "Node state not found for node id: " << nodeId.value;
        return std::nullopt;
    }
    return iter->second;
}

std::set<NodeId> GetDownstreamNodeIds(const GraphState& state, NodeId nodeId) {
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
