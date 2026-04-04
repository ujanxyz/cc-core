#include "ujcore/graph/GraphEngineImpl.h"

#include "absl/strings/str_cat.h"
#include "ujcore/data/AbslStringifies.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/graph/IdGenerator.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

absl::StatusOr<std::tuple<const plinfo::NodeInfo*, plstate::NodeState*>> InternalGetNodeInfoAndState(GraphState& state, const uint32_t nodeId) {
  const auto infoIter = state.node_infos.find(nodeId);
  if (infoIter == state.node_infos.end()) {
    return absl::InternalError(absl::StrCat("Node info lookup failed: ", nodeId));
  }
  const auto stateIter = state.node_states.find(nodeId);
  if (stateIter == state.node_states.end()) {
    return absl::InternalError(absl::StrCat("Node state lookup failed: ", nodeId));
  }
  return std::make_tuple(&infoIter->second, &stateIter->second);
}

absl::StatusOr<std::tuple<const plinfo::SlotInfo*, plstate::SlotState*>> InternalGetSlotInfoAndState(GraphState& state, const uint32_t nodeId, const std::string& slotName) {
  const plinfo::SlotId slotId = {nodeId, slotName};
  const auto infoIter = state.slot_infos.find(slotId);
  if (infoIter == state.slot_infos.end()) {
    return absl::InternalError(absl::StrCat("Slot info lookup failed: ", slotId));
  }
  const auto stateIter = state.slot_states.find(slotId);
  if (stateIter == state.slot_states.end()) {
    return absl::InternalError(absl::StrCat("Slot state lookup failed: ", slotId));
  }
  return std::make_tuple(&infoIter->second, &stateIter->second);
}

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
// Returns the deleted slotids.
std::set<plinfo::SlotId> InternalDeleteNodesGetOrphanEdges(const std::vector<uint32_t>& nodeIds, GraphState& state, std::set<uint32_t>& orphanEdges) {
  std::set<uint32_t> nodeIdsToDelete;
  for (const uint32_t rawNodeId : nodeIds) {
    nodeIdsToDelete.insert(rawNodeId);
    state.node_infos.erase(rawNodeId);
    state.node_states.erase(rawNodeId);
    // TODO: Put the node's adjacent edges in to 'orphan_edges'.
  }

  // Delete the slots of the deleted nodes.
  std::set<plinfo::SlotId> slotIdsToDelete;
  for (const auto& [slotId, slotInfo] : state.slot_infos) {
    if (nodeIdsToDelete.contains(slotId.parent)) {
      slotIdsToDelete.insert(slotId);
    }
  }
  return slotIdsToDelete;
}

// Returns the collection of actually deleted edges.
std::map<uint32_t, plinfo::EdgeInfo> DeleteEdges(const std::set<uint32_t>& edgeIds, GraphState& state) {
  std::map<uint32_t, plinfo::EdgeInfo> deletedEdges;
  for (const uint32_t rawEdgeId : edgeIds) {
    const auto iter = state.edge_infos.find(rawEdgeId);
    if (iter == state.edge_infos.end()) continue;
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

//--------------------------------------------------------------------------------------------------
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

absl::StatusOr<std::vector<plinfo::SlotInfo>> GraphEngineImpl::LookupNodeSlotInfos(
    const uint32_t nodeId,
    const std::vector<std::string>& slotNames) const {
  std::vector<plinfo::SlotInfo> infos;
  infos.reserve(slotNames.size());
  for (const std::string& slotName : slotNames) {
    const plinfo::SlotId slotId = {nodeId, slotName}; 
    const auto iter = state_.slot_infos.find(slotId);
    if (iter != state_.slot_infos.end()) {
      infos.push_back(iter->second);
    } else {
      return absl::InternalError(absl::StrCat("Slot info lookup failed: ", slotId));
    }
  }
  return infos;
}

absl::StatusOr<std::vector<std::pair<plinfo::SlotId, plstate::SlotState>>>
GraphEngineImpl::LookupSlotStates(const std::vector<plinfo::SlotId>& slotIds) {
  std::vector<std::pair<plinfo::SlotId, plstate::SlotState>> slotStates;
  slotStates.reserve(slotIds.size());
  for (const plinfo::SlotId& slotId : slotIds) {
    const auto iter = state_.slot_states.find(slotId);
    if (iter != state_.slot_states.end()) {
      slotStates.emplace_back(slotId, iter->second);
    } else {
      return absl::InternalError(absl::StrCat("Slot state lookup failed: ", slotId));
    }
  }
  return slotStates;
}

absl::StatusOr<plinfo::NodeInfo> GraphEngineImpl::AddNode(const data::FunctionInfo& fn_info) {
  const uint32_t rawId = ++(state_.idgen_state.next_node_id);
  const std::string alphanumId = GenSplitMix64OfLength(rawId + config_.nodeid_splitmix_offset, 10);
  plinfo::NodeInfo nodeInfo = {
    .rawId = rawId,
    .alnumid = alphanumId,
    .fnuri = fn_info.uri,
  };
  std::vector<plinfo::SlotInfo> slots = AddNodeSlotsInternal(fn_info, rawId, nodeInfo);
  // TODO: Add slots.
  for (auto&& slot : std::move(slots)) {
    const plinfo::SlotId slot_id = {rawId, slot.name};
    state_.slot_states[slot_id] = plstate::SlotState {
      .genId = 0,
    };
    state_.slot_infos[slot_id] = std::move(slot);
  }
  state_.node_infos[rawId] = nodeInfo;
  state_.node_states[rawId] = plstate::NodeState {
    .genId = 0,
  };
  return nodeInfo;
}

absl::StatusOr<plinfo::EdgeInfo> GraphEngineImpl::AddEdge(const uint32_t sourceNode, const std::string& sourceSlot, const uint32_t targetNode, const std::string& targetSlot) {
  const plinfo::NodeInfo* nodeInfo0 = nullptr;
  const plinfo::NodeInfo* nodeInfo1 = nullptr;
  plstate::NodeState* nodeState0 = nullptr;
  plstate::NodeState* nodeState1 = nullptr;
  // This quits if any node info or state lookup fails, which means the edge cannot be added.
  ASSIGN_OR_RETURN(std::tie(nodeInfo0, nodeState0), InternalGetNodeInfoAndState(state_, sourceNode));
  ASSIGN_OR_RETURN(std::tie(nodeInfo1, nodeState1), InternalGetNodeInfoAndState(state_, targetNode));

  const plinfo::SlotInfo* slotInfo0 = nullptr;
  const plinfo::SlotInfo* slotInfo1 = nullptr;
  plstate::SlotState* slotState0 = nullptr;
  plstate::SlotState* slotState1 = nullptr;

  // This quits if any slot info or state lookup fails, which means the edge cannot be added.
  ASSIGN_OR_RETURN(std::tie(slotInfo0, slotState0), InternalGetSlotInfoAndState(state_, sourceNode, sourceSlot));
  ASSIGN_OR_RETURN(std::tie(slotInfo1, slotState1), InternalGetSlotInfoAndState(state_, targetNode, targetSlot));

  if (!slotState1->inEdges.empty()) {
    return absl::InternalError("Target slot already has an edge");
  }

  const uint32_t rawId = static_cast<uint32_t>(++state_.idgen_state.next_edge_id);
  const std::string catid = absl::StrCat(nodeInfo0->alnumid, "$", sourceSlot, "--", nodeInfo1->alnumid, "$", targetSlot);
  const auto newEdge = plinfo::EdgeInfo {
    .id = rawId,
    .catid = catid,
    .node0 = sourceNode,
    .node1 = targetNode,
    .slot0 = sourceSlot,
    .slot1 = targetSlot,
  };
  state_.edge_infos[rawId] = newEdge;

  slotState0->outEdges.insert(rawId);
  slotState1->inEdges.insert(rawId);
  return newEdge;
}

absl::StatusOr<std::tuple<std::vector<uint32_t> /* edge ids */, std::set<plinfo::SlotId> /* deleted slot ids*/, std::set<plinfo::SlotId> /* affected slot ids */ >>
GraphEngineImpl::DeleteElements(const std::vector<uint32_t>& nodeIds, const std::vector<uint32_t>& edgeIds) {
  // The affected edges re the ones explicitly deleted or implicitly deleted due to node deletion.
  // We need to collect them together to properly update the states of their adjacent slots.
  std::set<uint32_t> affectedEdgeIds;
  std::set<plinfo::SlotId> slotIdsToDelete = InternalDeleteNodesGetOrphanEdges(nodeIds, state_, affectedEdgeIds);
  for (const uint32_t edgeId : edgeIds) {
    affectedEdgeIds.insert(edgeId);
  }

  // First delete the slots of the deleted nodes, so their state won't be updated when we update
  // the states of the adjacent slots of the deleted edges.
  for (const plinfo::SlotId& slotId : slotIdsToDelete) {
    state_.slot_infos.erase(slotId);
    state_.slot_states.erase(slotId);
  }

  // Delete the edges and collect the slot ids of those deleted edges.
  std::set<plinfo::SlotId> affectedSlotIds;
  std::vector<uint32_t> deletedEdgeIds;
  std::map<uint32_t, plinfo::EdgeInfo> deletedEdges = DeleteEdges(affectedEdgeIds, state_);
  for (const auto& [edgeId, edgeInfo] : deletedEdges) {
    // Update the incoming and outgoing edge ids at the source and target slot of the deleted edge.
    const plinfo::SlotId sourceSlotId = {edgeInfo.node0, edgeInfo.slot0};
    const plinfo::SlotId targetSlotId = {edgeInfo.node1, edgeInfo.slot1};

    auto sourceSlotStateIter = state_.slot_states.find(sourceSlotId);
    if (sourceSlotStateIter != state_.slot_states.end()) {
      sourceSlotStateIter->second.outEdges.erase(edgeId);
      affectedSlotIds.insert(sourceSlotId);
    }

    auto targetSlotStateIter = state_.slot_states.find(targetSlotId);
    if (targetSlotStateIter != state_.slot_states.end()) {
      targetSlotStateIter->second.inEdges.erase(edgeId);
      affectedSlotIds.insert(targetSlotId);
    }

    deletedEdgeIds.push_back(edgeInfo.id);
  }

  return std::make_tuple(deletedEdgeIds, slotIdsToDelete, affectedSlotIds);
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
  return deletedNodeIds.size() + deletedEdgeIds.size();
}

void GraphEngineImpl::AddAndResetTopoOrder(EngineOpResult& result) {
  if (topo_sort_order_.HasDirtyBitSet()) {
    result.topo_order = topo_sort_order_.CurrentOrder();
    topo_sort_order_.ClearDirtyBit();
  }
}

}  // namespace ujcore
