#pragma once

#include "ujcore/functions/NodeFunctionSpec.hpp"

#include <map>
#include <string>
#include <vector>

namespace ujcore {


struct SlotData {
    std::string parent_node;

    // "r" for read-only, "w" for create, "m" for mutable.
    std::string access;

    // E.g. "float[]", "point2d[]"
    std::string dtype;
};


    // Node fields except the id, which is available from the map key where the nodes are stores.
struct NodeData {
    // Identifies the node function used, e.g. "/fn/points-on-curve"
    std::string func_uri;

    // UI visible label.
    std::u8string label;

    // The function spec.
    NodeFunctionSpec spec;

    std::vector<std::string> slot_ids;
};

struct EdgeData {
    std::string srcSlot;
    std::string destSlot;

    // The node ids can also be derived by looking the slots.
    std::string srcNode;
    std::string destNode;
};


struct GraphState {
    std::map<std::string /* slot id */, SlotData> slots;
    std::map<std::string /* node id */, NodeData> nodes;
    std::map<std::string /* edge id */, EdgeData> edges;
};

}  // namespace ujcore
