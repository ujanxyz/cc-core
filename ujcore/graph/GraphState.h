#pragma once

#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"

#include <map>
#include <string>
#include <unordered_map>

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

    std::map<plinfo::SlotId, plinfo::SlotInfo> slot_infos;
    std::map<uint32_t /* node id (raw) */, plinfo::NodeInfo> node_infos;
    std::map<uint32_t /* edge id (raw) */, plinfo::EdgeInfo> edge_infos;

    // States (Dynamic data):
    std::map<plinfo::SlotId, plstate::SlotState> slot_states;
    std::map<uint32_t /* node id (raw) */, plstate::NodeState> node_states;
};

}  // namespace ujcore
