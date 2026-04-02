#include "ujcore/graph/GraphEngineImpl.h"

#include "absl/strings/str_cat.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/graph/IdGenerator.h"

namespace ujcore {
namespace {

std::vector<plinfo::SlotInfo> AddNodeSlotsInternal(const data::FunctionInfo& fnInfo, const uint32_t rawId, plinfo::NodeInfo& nodeInfo) {
    using ::ujcore::plinfo::SlotInfo;
    std::vector<SlotInfo> slots;
    slots.reserve(fnInfo.params.size());
    for (const data::ParamInfo& param : fnInfo.params) {
      SlotInfo slot = {
        .parent = rawId,
        .name = param.name,
        .dtype = param.dtype,
      };
      switch (param.access) {
        case data::ParamInfo::AccessEnum::I:
          slot.access = SlotInfo::AccessEnum::I;
          nodeInfo.ins.push_back(param.name);
          break;
        case data::ParamInfo::AccessEnum::O:
          slot.access = SlotInfo::AccessEnum::O;
          nodeInfo.outs.push_back(param.name);
          break;
        case data::ParamInfo::AccessEnum::M:
          slot.access = SlotInfo::AccessEnum::M;
          nodeInfo.inouts.push_back(param.name);
          break;
      }
      slots.push_back(std::move(slot));
    }

    return slots;
}

// Returns the set of actually deleted node ids.
void DeleteNodesGetOrphanEdges(const std::vector<std::string>& nodeIds, GraphState& state, std::set<uint32_t>& orphanEdges) {
  for (const std::string& nodeId : nodeIds) {
    const uint32_t rawNodeId = state.node_id_lookup[nodeId];
    if (rawNodeId < 0) continue;
    state.node_id_lookup.erase(nodeId);
    state.node_infos.erase(rawNodeId);
    state.node_states.erase(rawNodeId);
    // TODO: Put the node's adjacent edges in to 'orphan_edges'.
  }
}

// Returns the collection of actually deleted edges.
std::map<uint32_t, plinfo::EdgeInfo> DeleteEdges(const std::set<uint32_t>& edgeIds, GraphState& state) {
  std::map<uint32_t, plinfo::EdgeInfo> deletedEdges;
  for (const uint32_t rawEdgeId : edgeIds) {
    const auto iter = state.edge_infos.find(rawEdgeId);
    if (iter == state.edge_infos.end()) continue;
      state.edge_id_lookup.erase(iter->second.catid);
      // TODO: Queue any post-deletion action following the deletion of this edge.
      deletedEdges[rawEdgeId] = std::move(iter->second);
      state.edge_infos.erase(iter);
    }
    return deletedEdges;
}

template<class K, class T>
std::vector<T> GetValuesFromInfosMap(const std::map<K, T>& infosMap) {
  std::vector<T> result;
  result.reserve(infosMap.size());
  for (const auto& [_, info] : infosMap) {
    result.push_back(info);
  }
  return result;
}

}  // namespace

GraphEngineImpl::GraphEngineImpl() {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
    .edgeid_splitmix_offset = 412234ULL,
  };
}

std::vector<plinfo::NodeInfo> GraphEngineImpl::GetNodeInfos() const {
  return GetValuesFromInfosMap(state_.node_infos);
}

std::vector<plinfo::EdgeInfo> GraphEngineImpl::GetEdgeInfos() const {
  return GetValuesFromInfosMap(state_.edge_infos);
}

std::vector<plinfo::SlotInfo> GraphEngineImpl::GetSlotInfos() const {
  return GetValuesFromInfosMap(state_.slot_infos);
}

absl::StatusOr<plinfo::NodeInfo> GraphEngineImpl::AddNode(const data::FunctionInfo& fn_info) {
  const uint32_t rawId = ++(state_.idgen_state.next_node_id);
  const std::string alphanumId = GenSplitMix64OfLength(rawId + config_.nodeid_splitmix_offset, 10);
  plinfo::NodeInfo nodeInfo = {
    .id = rawId,
    .alnumid = alphanumId,
    .fnuri = fn_info.uri,
  };
  std::vector<plinfo::SlotInfo> slots = AddNodeSlotsInternal(fn_info, rawId, nodeInfo);
  // TODO: Add slots.
  for (auto&& slot : std::move(slots)) {
    const plinfo::SlotId slot_id = {rawId, slot.name};
    state_.slot_states[slot_id] = plstate::SlotState {
      .gen = 0,
    };
    state_.slot_infos[slot_id] = std::move(slot);
  }
  state_.node_infos[rawId] = nodeInfo;
  state_.node_id_lookup[alphanumId] = rawId;
  return nodeInfo;
}

absl::StatusOr<plinfo::EdgeInfo> GraphEngineImpl::AddEdge(const std::string& sourceNode, const std::string& sourceSlot, const std::string& targetNode, const std::string& targetSlot) {
  const uint32_t node0_id = state_.node_id_lookup[sourceNode];
  const uint32_t node1_id = state_.node_id_lookup[targetNode];
  if (node0_id == 0 || node1_id == 0) {
      return absl::InternalError("Node id lookup failed: " + sourceNode + ", " + targetNode);
  }
  const auto& slots = state_.slot_infos;

  auto iter0 = slots.find(std::make_tuple(node0_id, sourceSlot));
  if (iter0 == slots.end()) {
      return absl::InternalError(absl::StrCat("Slot lookup failed: ", sourceSlot, " in ", sourceNode));
  }

  auto iter1 = slots.find(std::make_tuple(node1_id, targetSlot));
  if (iter1 == slots.end()) {
      return absl::InternalError(absl::StrCat("Slot lookup failed: ", targetSlot, " in ", targetNode));
  }

  const uint32_t rawId = static_cast<uint32_t>(++state_.idgen_state.next_edge_id);
  const std::string catid = absl::StrCat(sourceNode, "$", sourceSlot, "--", targetNode, "$", targetSlot);
  const auto newEdge = plinfo::EdgeInfo {
    .id = rawId,
    .catid = catid,
    .node0 = node0_id,
    .node1 = node1_id,
    .slot0 = sourceSlot,
    .slot1 = targetSlot,
  };
  state_.edge_id_lookup[catid] = rawId;
  state_.edge_infos[rawId] = newEdge;
  return newEdge;
}

absl::StatusOr<std::vector<std::string> /* edge ids */>
GraphEngineImpl::DeleteElements(const std::vector<std::string>& nodeIds, const std::vector<std::string>& edgeIds) {
    std::set<uint32_t> uniqueEdgeIds;
  DeleteNodesGetOrphanEdges(nodeIds, state_, uniqueEdgeIds);

  for (const std::string& edgeId : edgeIds) {
    const uint32_t rawEdgeId = state_.edge_id_lookup[edgeId];
    if (rawEdgeId > 0) uniqueEdgeIds.insert(rawEdgeId);
  }

  std::map<uint32_t, plinfo::EdgeInfo> deletedEdges = DeleteEdges(uniqueEdgeIds, state_);
  std::vector<std::string> deletedEdgeIds;
  for (const auto& [id, edge] : deletedEdges) {
    deletedEdgeIds.push_back(edge.catid);
  }
  return deletedEdgeIds;
}

absl::StatusOr<int> GraphEngineImpl::ClearGraph() {
  std::vector<uint32_t> deletedNodeIds;
  std::vector<uint32_t> deletedEdgeIds;

  // Remove the edges and collect their ids.
  {
    decltype(state_.edge_infos) edges_map;
    edges_map.swap(state_.edge_infos);
    deletedEdgeIds.reserve(edges_map.size());
    for (const auto& [id, _] : edges_map) {
      deletedEdgeIds.push_back(id);
    }
  }
  // Remove the nodes and collect their ids.
  {
    decltype(state_.node_infos) nodes_map;
    nodes_map.swap(state_.node_infos);
    deletedNodeIds.reserve(nodes_map.size());
    for (const auto& [id, _] : nodes_map) {
      deletedNodeIds.push_back(id);
    }
  }

  state_.node_states.clear();
  state_.slot_states.clear();
  state_.node_id_lookup.clear();
  state_.edge_id_lookup.clear();
  return deletedNodeIds.size() + deletedEdgeIds.size();
}

void GraphEngineImpl::AddAndResetTopoOrder(EngineOpResult& result) {
  if (topo_sort_order_.HasDirtyBitSet()) {
    result.topo_order = topo_sort_order_.CurrentOrder();
    topo_sort_order_.ClearDirtyBit();
  }
}

}  // namespace ujcore
