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
  const auto infoIter = state.nodeInfos.find(nodeId);
  if (infoIter == state.nodeInfos.end()) {
    return absl::InternalError(absl::StrCat("Node info lookup failed: ", nodeId));
  }
  const auto stateIter = state.nodeStates.find(nodeId);
  if (stateIter == state.nodeStates.end()) {
    return absl::InternalError(absl::StrCat("Node state lookup failed: ", nodeId));
  }
  return std::make_tuple(&infoIter->second, &stateIter->second);
}

absl::StatusOr<std::tuple<const plinfo::SlotInfo*, plstate::SlotState*>>
InternalGetSlotInfoAndState(GraphState& state, const NodeId nodeId, const std::string& slotName) {
  const SlotId slotId = {nodeId, slotName};
  const auto infoIter = state.slotInfos.find(slotId);
  if (infoIter == state.slotInfos.end()) {
    return absl::InternalError(absl::StrCat("Slot info lookup failed: ", slotId));
  }
  const auto stateIter = state.slotStates.find(slotId);
  if (stateIter == state.slotStates.end()) {
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
    state.nodeInfos.erase(nodeId);
    state.nodeStates.erase(nodeId);
    // TODO: Put the node's adjacent edges in to 'orphan_edges'.
  }

  // Delete the slots of the deleted nodes.
  std::set<SlotId> slotIdsToDelete;
  for (const auto& [slotId, slotInfo] : state.slotInfos) {
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
    const auto iter = state.edgeInfos.find(edgeId);
    if (iter == state.edgeInfos.end()) continue;
      // TODO: Queue any post-deletion action following the deletion of this edge.
      deletedEdges[edgeId] = std::move(iter->second);
      state.edgeInfos.erase(iter);
    }
    return deletedEdges;
}

}  // namespace

//--------------------------------------------------------------------------------------------------
GraphBuilder::GraphBuilder(GraphState& state, TopoSortOrder& topoSorter): state_(state), topoSorter_(topoSorter) {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
  };
}

absl::StatusOr<std::vector<plinfo::SlotInfo>> GraphBuilder::LookupNodeSlotInfos(
    const NodeId nodeId,
    const std::vector<std::string>& slotNames) const {
  std::vector<plinfo::SlotInfo> infos;
  infos.reserve(slotNames.size());
  for (const std::string& slotName : slotNames) {
    const SlotId slotId = {nodeId, slotName}; 
    const auto iter = state_.slotInfos.find(slotId);
    if (iter != state_.slotInfos.end()) {
      infos.push_back(iter->second);
    } else {
      return absl::InternalError(absl::StrCat("Slot info lookup failed: ", slotId));
    }
  }
  return infos;
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
    .ntype = plinfo::NodeInfo::NodeType::FN,
    .uri = funcSpec->uri,
  };
  std::vector<plinfo::SlotInfo> slots = AddNodeSlotsInternal(*funcSpec, nodeId, nodeInfo);
  for (auto&& slot : std::move(slots)) {
    const SlotId slot_id = {nodeId, slot.name};
    state_.slotStates[slot_id] = plstate::SlotState {
      .genId = 0,
    };
    state_.slotInfos[slot_id] = std::move(slot);
  }
  state_.nodeInfos[nodeId] = nodeInfo;
  state_.nodeStates[nodeId] = plstate::NodeState {
    .label = funcSpec->label,
    .connected = plstate::NodeState::ConnectedState::WAIT,
    .genId = 0,
  };
  topoSorter_.AddNode(nodeId);
  return nodeInfo;
}

absl::StatusOr<std::tuple<plinfo::NodeInfo, plinfo::SlotInfo>>
GraphBuilder::AddIONode(const std::string& dtype, bool isOutput) {
  const NodeId nodeId (++state_.idgen_state.next_node_id);
  const std::string alphanumId = GenSplitMix64OfLength(nodeId.value + config_.nodeid_splitmix_offset, 10);
  plinfo::NodeInfo nodeInfo = {
    .rawId = nodeId,
    .alnumid = alphanumId,
    .ntype = isOutput ? plinfo::NodeInfo::NodeType::OUT : plinfo::NodeInfo::NodeType::IN,
    .uri = absl::StrCat(isOutput ? "/$OUT/" : "/$IN/", dtype),
  };
  // Graph input node has an output slot, and graph output node has an input slot.
  // Since the IO nodes have a single slot, it's name is taken as "$out" for input
  // node and "$in" for output node.
  const plinfo::SlotInfo slotInfo = {
    .parent = nodeId,
    .name = isOutput ? "$in" : "$out",
    .dtype = dtype,
    .access = isOutput ? plinfo::SlotInfo::AccessEnum::I : plinfo::SlotInfo::AccessEnum::O,
  };
  const SlotId slot_id = {nodeId, slotInfo.name};
  state_.slotStates[slot_id] = plstate::SlotState {
    .genId = 0,
  };
  state_.slotInfos[slot_id] = slotInfo;
  if (isOutput) {
    nodeInfo.ins.push_back(slotInfo.name);
  } else {
    nodeInfo.outs.push_back(slotInfo.name);
  }
  state_.nodeInfos[nodeId] = nodeInfo;
  state_.nodeStates[nodeId] = plstate::NodeState {
    .label = isOutput ? absl::StrCat("Output: ", dtype) : absl::StrCat("Input: ", dtype),
    .connected = plstate::NodeState::ConnectedState::WAIT,
    .genId = 0,
  };
  topoSorter_.AddNode(nodeId);
  return std::make_tuple(nodeInfo, slotInfo);
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
  state_.edgeInfos[edgeId] = newEdge;

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
    state_.slotInfos.erase(slotId);
    state_.slotStates.erase(slotId);
  }

  // Delete the edges and collect the slot ids of those deleted edges.
  std::set<SlotId> affectedSlotIds;
  std::vector<EdgeId> deletedEdgeIds;
  std::map<EdgeId, plinfo::EdgeInfo> deletedEdges = InternalDeleteEdges(affectedEdgeIds, state_);
  for (const auto& [edgeId, edgeInfo] : deletedEdges) {
    // Update the incoming and outgoing edge ids at the source and target slot of the deleted edge.
    const SlotId sourceSlotId = {edgeInfo.node0, edgeInfo.slot0};
    const SlotId targetSlotId = {edgeInfo.node1, edgeInfo.slot1};

    auto sourceSlotStateIter = state_.slotStates.find(sourceSlotId);
    if (sourceSlotStateIter != state_.slotStates.end()) {
      sourceSlotStateIter->second.outEdges.erase(edgeId);
      affectedSlotIds.insert(sourceSlotId);
    }

    auto targetSlotStateIter = state_.slotStates.find(targetSlotId);
    if (targetSlotStateIter != state_.slotStates.end()) {
      targetSlotStateIter->second.inEdges.erase(edgeId);
      affectedSlotIds.insert(targetSlotId);
    }

    deletedEdgeIds.push_back(edgeInfo.id);

    // The edge should be deleted from topo-order if there is no more edge between the same
    // source and target node. Check that and delete the node to node dependency.
    auto downstreamNodes = GraphUtils::GetDownstreamNodeIds(state_, edgeInfo.node0);
    if (!downstreamNodes.contains(edgeInfo.node1)) {
      topoSorter_.RemoveEdge(edgeInfo.node0, edgeInfo.node1);
    }
  }

  for (const NodeId nid : nodeIds) {
    topoSorter_.RemoveNode(nid);
  }

  return std::make_tuple(deletedEdgeIds, slotIdsToDelete, affectedSlotIds);
}

absl::Status GraphBuilder::ClearManualInputs(const std::vector<SlotId>& slotIds) {
  for (const SlotId& slotId : slotIds) {
    auto iter = state_.slotStates.find(slotId);
    if (iter != state_.slotStates.end()) {
      iter->second.manual.reset();
    } else {
      return absl::InternalError(absl::StrCat("Slot state lookup failed: ", slotId));
    }
  }
  return absl::OkStatus();
}

absl::Status GraphBuilder::SetManualInputs(const std::map<SlotId, std::string /* encoded data */>& manualInputs) {
  for (const auto& [slotId, encodedData] : manualInputs) {
    RETURN_IF_NOT_FOUND_IN_MAP(const plinfo::NodeInfo& nodeInfo, state_.nodeInfos, slotId.parent);
    RETURN_IF_NOT_FOUND_IN_MAP(const plinfo::SlotInfo& slotInfo, state_.slotInfos, slotId);
    RETURN_IF_NOT_FOUND_IN_MAP(plstate::SlotState& slotState, state_.slotStates, slotId);
    
    if (nodeInfo.ntype == plinfo::NodeInfo::NodeType::FN) {
      if (slotInfo.access == plinfo::SlotInfo::AccessEnum::O) {
        return absl::InvalidArgumentError(
            absl::StrCat("Cannot set manual input for output slot: ", slotId));
      }
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Manual input cannot be set for graph input / output slot: ", slotId));
    }

    slotState.manual = plstate::EncodedData {
      .payload = encodedData,
    };
  }
  return absl::OkStatus();
}

absl::StatusOr<std::map<SlotId, std::optional<std::string> /* encoded data */>>
GraphBuilder::FetchManualInputs(const std::vector<SlotId>& slotIds) const {
  std::map<SlotId, std::optional<std::string>> result;
  for (const SlotId& slotId : slotIds) {
    RETURN_IF_NOT_FOUND_IN_MAP(const plstate::SlotState& slotState, state_.slotStates, slotId);
    if (slotState.manual.has_value()) {
      result[slotId] = slotState.manual->payload;
    } else {
      result[slotId] = std::nullopt;
    }
  }
  return result;
}

absl::Status GraphBuilder::SetGraphInputs(const std::vector<std::tuple<NodeId, std::string /* encoded data */>>& graphInputs) {
  for (const auto& [nodeId, encodedData] : graphInputs) {
    LOG(INFO) << "Setting graph input for node " << nodeId << " with data: " << encodedData;
    RETURN_IF_NOT_FOUND_IN_MAP(const plinfo::NodeInfo& nodeInfo, state_.nodeInfos, nodeId);
    RETURN_IF_NOT_FOUND_IN_MAP(plstate::NodeState& nodeState, state_.nodeStates, nodeId);
    
    if (nodeInfo.ntype != plinfo::NodeInfo::NodeType::IN) {
      return absl::InvalidArgumentError(
          absl::StrCat("Graph input data can only be set for input nodes. Invalid node id: ", nodeId));
    }

    nodeState.ioData = plstate::EncodedData {
      .payload = encodedData,
    };
  }
  return absl::OkStatus();
}

absl::Status GraphBuilder::ClearGraphInputs(const std::vector<NodeId>& nodeIds) {
  for (const NodeId& nodeId : nodeIds) {
    RETURN_IF_NOT_FOUND_IN_MAP(plstate::NodeState& nodeState, state_.nodeStates, nodeId);
    nodeState.ioData.reset();
  }
  return absl::OkStatus();
}

absl::StatusOr<int> GraphBuilder::ClearGraph() {
  std::vector<NodeId> deletedNodeIds;
  std::vector<EdgeId> deletedEdgeIds;

  // Remove the edges and collect their ids.
  {
    decltype(state_.edgeInfos) edges_map;
    edges_map.swap(state_.edgeInfos);
    deletedEdgeIds.reserve(edges_map.size());
    for (const auto& [edgeId, _] : edges_map) {
      deletedEdgeIds.push_back(edgeId);
    }
  }
  // Remove the nodes and collect their ids.
  {
    decltype(state_.nodeInfos) nodes_map;
    nodes_map.swap(state_.nodeInfos);
    deletedNodeIds.reserve(nodes_map.size());
    for (const auto& [id, _] : nodes_map) {
      deletedNodeIds.push_back(id);
    }
  }

  state_.nodeStates.clear();
  state_.slotStates.clear();
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
