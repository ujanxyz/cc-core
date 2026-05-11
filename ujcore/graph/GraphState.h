#pragma once

#include "ujcore/graph/TopoSortState.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphTypes.h"

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

    std::map<SlotId, grph::SlotInfo> slotInfos;
    std::map<NodeId, grph::NodeInfo> nodeInfos;
    std::map<EdgeId, grph::EdgeInfo> edgeInfos;

    // States (Dynamic data):
    std::map<SlotId, grph::SlotState> slotStates;
    std::map<NodeId, grph::NodeState> nodeStates;
};

}  // namespace ujcore
