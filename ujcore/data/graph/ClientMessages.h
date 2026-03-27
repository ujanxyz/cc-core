#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cppschema/common/visitor_macros.h"

namespace ujcore::data {

struct AddEdgeEntry {
    std::string node0;  // The source node
    std::string slot0;  // The source slot
    std::string node1;  // The target node
    std::string slot1;  // The target slot

    DEFINE_STRUCT_VISITOR_FUNCTION(node0, slot0, node1, slot1);
};

struct NodeAndEdgeIds {
    std::vector<uint32_t> node_ids;
    std::vector<uint32_t> edge_ids;

    DEFINE_STRUCT_VISITOR_FUNCTION(node_ids, edge_ids);
};

struct FindSlotEntry {
    // If set this request is to find a matching source slot in the new node, else find a target
    // slot in the new node.
    bool findsource {false};

    std::string node;  // The other node
    std::string slot;  // The other slot

    DEFINE_STRUCT_VISITOR_FUNCTION(findsource, node, slot);
};

}  // namespace ujcore::data
