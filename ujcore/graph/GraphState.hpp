#pragma once

#include "ujcore/functions/NodeFunctionSpec.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ujcore {

struct SlotData {
    // Id of this slot.
    std::string id;

    // The parent node id.
    std::string node_id;

    // The data type of what flows through the slot.
    std::string dtype;

    // Is it a output slot, or an input slot.
    bool is_output {false};

    // Overridden manual input, applicable only to input slots.
    std::optional<std::string> payload;

    // The edges connected to/from this slot.
    std::vector<std::string> edge_ids;

    // Last modified timestamp or generation id.
    int64_t modified {0LL};
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

struct IdGeneratorState {
    uint64_t next_node_id {0ULL};
    uint64_t next_edge_id {412234ULL};
    uint64_t next_slot_id {987123ULL};
};


struct GraphState {
    std::map<std::string /* slot id */, SlotData> slots;
    std::map<std::string /* node id */, NodeData> nodes;
    std::map<std::string /* edge id */, EdgeData> edges;
    IdGeneratorState idgen_state;
};

}  // namespace ujcore
