#include "ujcore/graph/GraphBuilder.h"

#include "absl/strings/str_cat.h"
#include "ujcore/data/AbslStringifies.h"  // IWYU pragma: keep
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/graph/IdGenerator.h"
#include "ujcore/utils/status_macros.h"
#include "ujcore/function/FunctionSpec.h"

namespace ujcore {
namespace {

absl::StatusOr<std::tuple<const plinfo::NodeInfo*, plstate::NodeState*>>
InternalGetNodeInfoAndState(GraphState& state, const NodeId nodeId) {
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

absl::StatusOr<std::tuple<const plinfo::SlotInfo*, plstate::SlotState*>>
InternalGetSlotInfoAndState(GraphState& state, const NodeId nodeId, const std::string& slotName) {
  const SlotId slotId = {nodeId, slotName};
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

std::vector<plinfo::SlotInfo>
AddNodeSlotsInternal(const FunctionSpec& fnInfo, const NodeId nodeId, plinfo::NodeInfo& nodeInfo) {
    using ::ujcore::plinfo::SlotInfo;
    std::vector<SlotInfo> slots;
    slots.reserve(fnInfo.params.size());
    for (const FuncParamSpec& param : fnInfo.params) {
      SlotInfo slot = {
        .parent = nodeId,
        .name = param.name,
        .dtype = AttributeDataTypeToStr(param.dtype),
      };
      switch (param.access) {
        case FuncParamAccess::kInput:
          slot.access = SlotInfo::AccessEnum::I;
          nodeInfo.ins.push_back(param.name);
          break;
        case FuncParamAccess::kOutput:
          slot.access = SlotInfo::AccessEnum::O;
          nodeInfo.outs.push_back(param.name);
          break;
        case FuncParamAccess::kInOut:
          slot.access = SlotInfo::AccessEnum::M;
          nodeInfo.inouts.push_back(param.name);
          break;
        case FuncParamAccess::kUnknown:
          LOG(FATAL);
      }
      slots.push_back(std::move(slot));
    }

    return slots;
}

// Returns the set of actually deleted node ids.
// Returns the deleted slotids.
std::set<SlotId> InternalDeleteNodesGetOrphanEdges(const std::vector<NodeId>& nodeIds, GraphState& state, std::set<EdgeId>& orphanEdges) {
  std::set<NodeId> nodeIdsToDelete;
  for (const NodeId nodeId : nodeIds) {
    nodeIdsToDelete.insert(nodeId);
    state.node_infos.erase(nodeId);
    state.node_states.erase(nodeId);
    // TODO: Put the node's adjacent edges in to 'orphan_edges'.
  }

  // Delete the slots of the deleted nodes.
  std::set<SlotId> slotIdsToDelete;
  for (const auto& [slotId, slotInfo] : state.slot_infos) {
    if (nodeIdsToDelete.contains(NodeId(slotId.parent))) {
      slotIdsToDelete.insert(slotId);
    }
  }
  return slotIdsToDelete;
}

// Returns the collection of actually deleted edges.
std::map<EdgeId, plinfo::EdgeInfo> InternalDeleteEdges(const std::set<EdgeId>& edgeIds, GraphState& state) {
  std::map<EdgeId, plinfo::EdgeInfo> deletedEdges;
  for (const EdgeId edgeId : edgeIds) {
    const auto iter = state.edge_infos.find(edgeId);
    if (iter == state.edge_infos.end()) continue;
      // TODO: Queue any post-deletion action following the deletion of this edge.
      deletedEdges[edgeId] = std::move(iter->second);
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
GraphBuilder::GraphBuilder(GraphState& state, TopoSortOrder& topoSorter): state_(state), topoSorter_(topoSorter) {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
  };
}

std::vector<plinfo::NodeInfo> GraphBuilder::GetNodeInfos() const {
  return GetValuesFromInfosMap(state_.node_infos);
}

std::vector<plinfo::EdgeInfo> GraphBuilder::GetEdgeInfos() const {
  return GetValuesFromInfosMap(state_.edge_infos);
}

std::vector<plinfo::SlotInfo> GraphBuilder::GetSlotInfos() const {
  return GetValuesFromInfosMap(state_.slot_infos);
}

absl::StatusOr<std::vector<plinfo::SlotInfo>> GraphBuilder::LookupNodeSlotInfos(
    const NodeId nodeId,
    const std::vector<std::string>& slotNames) const {
  std::vector<plinfo::SlotInfo> infos;
  infos.reserve(slotNames.size());
  for (const std::string& slotName : slotNames) {
    const SlotId slotId = {nodeId, slotName}; 
    const auto iter = state_.slot_infos.find(slotId);
    if (iter != state_.slot_infos.end()) {
      infos.push_back(iter->second);
    } else {
      return absl::InternalError(absl::StrCat("Slot info lookup failed: ", slotId));
    }
  }
  return infos;
}

absl::StatusOr<std::vector<std::pair<SlotId, plstate::SlotState>>>
GraphBuilder::LookupSlotStates(const std::vector<SlotId>& slotIds) {
  std::vector<std::pair<SlotId, plstate::SlotState>> slotStates;
  slotStates.reserve(slotIds.size());
  for (const SlotId& slotId : slotIds) {
    const auto iter = state_.slot_states.find(slotId);
    if (iter != state_.slot_states.end()) {
      slotStates.emplace_back(slotId, iter->second);
    } else {
      return absl::InternalError(absl::StrCat("Slot state lookup failed: ", slotId));
    }
  }
  return slotStates;
}

absl::StatusOr<plinfo::NodeInfo> GraphBuilder::AddFuncNode(const FunctionInfo& fnInfo) {
  auto& registry = FunctionRegistry::GetInstance();
  std::unique_ptr<FunctionSpec> funcSpec = registry.GetSpecFromUri(fnInfo.uri);
  if (funcSpec == nullptr) {
    return absl::NotFoundError(absl::StrCat("Function not found: ", fnInfo.uri));
  }
  const NodeId nodeId (++state_.idgen_state.next_node_id);
  const std::string alphanumId = GenSplitMix64OfLength(nodeId.value + config_.nodeid_splitmix_offset, 10);
  plinfo::NodeInfo nodeInfo = {
    .rawId = nodeId,
    .alnumid = alphanumId,
    .fnuri = funcSpec->uri,
  };
  std::vector<plinfo::SlotInfo> slots = AddNodeSlotsInternal(*funcSpec, nodeId, nodeInfo);
  for (auto&& slot : std::move(slots)) {
    const SlotId slot_id = {nodeId, slot.name};
    state_.slot_states[slot_id] = plstate::SlotState {
      .genId = 0,
    };
    state_.slot_infos[slot_id] = std::move(slot);
  }
  state_.node_infos[nodeId] = nodeInfo;
  state_.node_states[nodeId] = plstate::NodeState {
    .label = funcSpec->label,
    .connected = plstate::NodeState::ConnectedState::WAIT,
    .genId = 0,
  };
  topoSorter_.AddNode(nodeId);
  return nodeInfo;
}

absl::StatusOr<plinfo::EdgeInfo> GraphBuilder::AddEdge(const NodeId sourceNode, const std::string& sourceSlot, const NodeId targetNode, const std::string& targetSlot) {
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

  const EdgeId edgeId (++state_.idgen_state.next_edge_id);
  const std::string catid = absl::StrCat(nodeInfo0->alnumid, "$", sourceSlot, "--", nodeInfo1->alnumid, "$", targetSlot);
  const auto newEdge = plinfo::EdgeInfo {
    .id = edgeId,
    .catid = catid,
    .node0 = sourceNode,
    .node1 = targetNode,
    .slot0 = sourceSlot,
    .slot1 = targetSlot,
  };
  state_.edge_infos[edgeId] = newEdge;

  slotState0->outEdges.insert(edgeId);
  slotState1->inEdges.insert(edgeId);
  topoSorter_.AddEdge(sourceNode, targetNode);
  return newEdge;
}

absl::StatusOr<std::tuple<std::vector<EdgeId>, std::set<SlotId> /* deleted slot ids*/, std::set<SlotId> /* affected slot ids */ >>
GraphBuilder::DeleteElements(const std::vector<NodeId>& nodeIds, const std::vector<EdgeId>& edgeIds) {
  // The affected edges re the ones explicitly deleted or implicitly deleted due to node deletion.
  // We need to collect them together to properly update the states of their adjacent slots.
  std::set<EdgeId> affectedEdgeIds;
  std::set<SlotId> slotIdsToDelete = InternalDeleteNodesGetOrphanEdges(nodeIds, state_, affectedEdgeIds);
  for (const EdgeId edgeId : edgeIds) {
    affectedEdgeIds.insert(edgeId);
  }

  // First delete the slots of the deleted nodes, so their state won't be updated when we update
  // the states of the adjacent slots of the deleted edges.
  for (const SlotId& slotId : slotIdsToDelete) {
    state_.slot_infos.erase(slotId);
    state_.slot_states.erase(slotId);
  }

  // Delete the edges and collect the slot ids of those deleted edges.
  std::set<SlotId> affectedSlotIds;
  std::vector<EdgeId> deletedEdgeIds;
  std::map<EdgeId, plinfo::EdgeInfo> deletedEdges = InternalDeleteEdges(affectedEdgeIds, state_);
  for (const auto& [edgeId, edgeInfo] : deletedEdges) {
    // Update the incoming and outgoing edge ids at the source and target slot of the deleted edge.
    const SlotId sourceSlotId = {edgeInfo.node0, edgeInfo.slot0};
    const SlotId targetSlotId = {edgeInfo.node1, edgeInfo.slot1};

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

    // The edge should be deleted from topo-order if there is no more edge between the same
    // source and target node. Check that and delete the node to node dependency.
    auto downstreamNodes = GetDownstreamNodeIds(state_, edgeInfo.node0);
    if (!downstreamNodes.contains(edgeInfo.node1)) {
      topoSorter_.RemoveEdge(edgeInfo.node0, edgeInfo.node1);
    }
  }

  for (const NodeId nid : nodeIds) {
    topoSorter_.RemoveNode(nid);
  }

  return std::make_tuple(deletedEdgeIds, slotIdsToDelete, affectedSlotIds);
}

absl::StatusOr<int> GraphBuilder::ClearGraph() {
  std::vector<NodeId> deletedNodeIds;
  std::vector<EdgeId> deletedEdgeIds;

  // Remove the edges and collect their ids.
  {
    decltype(state_.edge_infos) edges_map;
    edges_map.swap(state_.edge_infos);
    deletedEdgeIds.reserve(edges_map.size());
    for (const auto& [edgeId, _] : edges_map) {
      deletedEdgeIds.push_back(edgeId);
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

absl::StatusOr<std::vector<FunctionInfo>>
GraphBuilder::GetAvailableFuncInfos() const {
  auto& registry = FunctionRegistry::GetInstance();
  const std::vector<std::string> allUris = registry.GetAllUris();

  std::vector<FunctionInfo> funcInfos;
  funcInfos.reserve(allUris.size());

  for (const auto& uri :  allUris) {
    auto spec = registry.GetSpecFromUri(uri);
    if (spec == nullptr) continue;
    funcInfos.push_back(FunctionInfo {
      .uri = uri,
      .label = spec->label,
      .desc = spec->desc,
    });
  }
  return funcInfos;
}

}  // namespace ujcore
