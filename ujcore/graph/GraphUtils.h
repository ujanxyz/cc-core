#pragma once

#include <optional>
#include <set>

#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"

namespace ujcore {

// Get the current state info of a node, given the graph state and the node id.
std::optional<plstate::NodeState> GetNodeState(const GraphState& state, NodeId nodeId);

// Given the graph state and a (source) node id, collect the unique
// list of direct downstream nodes, i.e. the nodes which are connected
// by an edge from the given node.
// This is calculated by iterating through the output and mutable slots
// and collecting the unique downstream node ids.
std::set<NodeId> GetDownstreamNodeIds(const GraphState& state, NodeId nodeId);

}  // namespace ujcore
