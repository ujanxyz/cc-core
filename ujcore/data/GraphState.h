#pragma once

#include "ujcore/data/TopoSortState.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"

#include <map>

namespace ujcore {

struct IdGeneratorState {
    uint32_t next_node_id {0};
    uint32_t next_edge_id {0};
};

struct GraphConfig {
    int64_t nodeid_splitmix_offset {0};
};

struct GraphState {
    IdGeneratorState idgen_state;
    TopoSortState topoSortState;

    std::map<SlotId, plinfo::SlotInfo> slotInfos;
    std::map<NodeId, plinfo::NodeInfo> nodeInfos;
    std::map<EdgeId, plinfo::EdgeInfo> edgeInfos;

    // States (Dynamic data):
    std::map<SlotId, plstate::SlotState> slotStates;
    std::map<NodeId, plstate::NodeState> nodeStates;
};

}  // namespace ujcore
