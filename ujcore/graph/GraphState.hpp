#pragma once

#include "ujcore/data/graph/GraphEdge.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace ujcore {

struct IdGeneratorState {
    uint32_t next_node_id {0};
    uint32_t next_edge_id {0};
};

struct GraphConfig {
    int64_t nodeid_splitmix_offset {0};
    int64_t edgeid_splitmix_offset {0};
};

struct GraphState {
    IdGeneratorState idgen_state;

    std::map<uint32_t /* node id */, data::GraphNode> nodes_map;
    std::map<uint32_t /* edge id */, data::GraphEdge> edges_map;
    std::map<data::SlotId, data::GraphSlot> slots;

    // Maps alpha-num to raw node id.
    std::unordered_map<std::string, uint32_t> node_id_lookup;

};

}  // namespace ujcore
