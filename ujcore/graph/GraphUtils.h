#pragma once

#include <optional>
#include <set>

#include "absl/status/statusor.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"

namespace ujcore {

class GraphUtils {
public:
    // The following methods collect the info entries from the map values.
    static std::vector<plinfo::NodeInfo> GetAllNodeInfos(const GraphState& state);
    static std::vector<plinfo::EdgeInfo> GetAllEdgeInfos(const GraphState& state);
    static std::vector<plinfo::SlotInfo> GetAllSlotInfos(const GraphState& state);

    // Looks up the current states of the nodes. Returns a map from node id to node state.
    static absl::StatusOr<std::map<NodeId, plstate::NodeState>> LookupNodeStates(const GraphState& state, const std::vector<NodeId>& nodeIds);

    // Looks up the current states of the slots. Returns a map from slot id to slot state.
    static absl::StatusOr<std::map<SlotId, plstate::SlotState>> LookupSlotStates(const GraphState& state, const std::vector<SlotId>& slotIds);


    // Get the encoded data for the given graph input node.
    // Returns nullopt if not found or not an input node.
    static std::optional<plstate::EncodedData> GetNodeIoData(const GraphState& state, NodeId nodeId);

    // Get a copy of the current state state of a node, given the graph state and the node id.
    static std::optional<plstate::NodeState> CopyNodeState(const GraphState& state, NodeId nodeId);

    // Get (copies of) the slot infos of a given node in the graph, given the node id.
    static std::vector<plinfo::SlotInfo> CopyAllSlotInfos(const GraphState& state, NodeId nodeId);

    // Given the graph state and a (source) node id, collect the unique
    // list of direct downstream nodes, i.e. the nodes which are connected
    // by an edge from the given node.
    // This is calculated by iterating through the output and mutable slots
    // and collecting the unique downstream node ids.
    static std::set<NodeId> GetDownstreamNodeIds(const GraphState& state, NodeId nodeId);

private:
    // No instantiation, all static methods.
    GraphUtils() = delete;
};

}  // namespace ujcore
