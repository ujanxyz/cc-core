#pragma once

#include <optional>
#include <set>

#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"

namespace ujcore {

class GraphUtils {
public:
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

    // static std::tuple<std::set<NodeId> /* incoming edge ids*/, std::set<NodeId> /* outgoing edge ids */>
    // GetAdjacentEdgeIds(const plstate::SlotState);
private:
    // No instantiation, all static methods.
    GraphUtils() = delete;
};

}  // namespace ujcore
