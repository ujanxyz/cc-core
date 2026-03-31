#include "ujcore/graph/GraphEngineImpl.h"

#include "ujcore/data/utils/CreateNodeSlots.h"
#include "ujcore/graph/GraphOpsContext.h"
#include "ujcore/graph/IdGenerator.h"

namespace ujcore {
namespace {

absl::StatusOr<data::GraphNode> CreateNode(GraphOpsContext& ctx, const data::FunctionInfo& fn_info) {
    const GraphConfig& config = ctx.config;
    GraphState& state = ctx.state;
    // auto* const toposort_order = ctx.toposort_order;

    const uint32_t raw_id = ++(state.idgen_state.next_node_id);
    const std::string alphanum_id = GenSplitMix64OfLength(raw_id + config.nodeid_splitmix_offset, 10);

    auto slots_or = data::CreateNodeSlots(fn_info, raw_id, alphanum_id);
    if (!slots_or.ok()) {
        return std::move(slots_or).status();
    }
    std::vector<data::GraphSlot> new_slots = std::move(slots_or).value();

    std::vector<std::string> slot_names;
    slot_names.reserve(new_slots.size());
    for (const auto& slot : new_slots) {
        slot_names.push_back(slot.name);
        state.slots[std::make_tuple(slot.parent, slot.name)] = std::move(slot);
    }

    data::GraphNode graph_node = {
        .id = raw_id,
        .alnumid = alphanum_id,
        .fnuri = fn_info.uri,
        .slots = slot_names,
    };

    state.nodes_map[raw_id] = graph_node;
    state.node_id_lookup[alphanum_id] = raw_id;
    // toposort_order->AddNode(alphanum_id);
    return graph_node;
}

absl::StatusOr<std::vector<data::GraphEdge>> AddEdgesInternal(GraphOpsContext& ctx, const std::vector<data::AddEdgeEntry>& entries) {
    GraphState& state = ctx.state;
    const auto& slots = state.slots;
    std::vector<data::GraphEdge> edges;
    for (const auto& entry : entries) {
        const uint32_t node0_id = state.node_id_lookup[entry.node0];
        const uint32_t node1_id = state.node_id_lookup[entry.node1];
        if (node0_id == 0 || node1_id == 0) {
            return absl::InternalError("Node id lookup failed: " + entry.node0 + ", " + entry.node1);
        }
        auto iter0 = slots.find(std::make_tuple(node0_id, entry.slot0));
        auto iter1 = slots.find(std::make_tuple(node1_id, entry.slot1));
        if (iter0 == slots.end() || iter1 == slots.end()) {
            return absl::InternalError("Slot lookup failed, in node# " + entry.node0);
        }
        const uint32_t raw_id = static_cast<uint32_t>(++state.idgen_state.next_edge_id);
        const auto new_edge = data::GraphEdge {
            .id = raw_id,
            .node0 = node0_id,
            .node1 = node1_id,
            .slot0 = entry.slot0,
            .slot1 = entry.slot1,
        };
        state.edges_map[raw_id] = new_edge;
        edges.push_back(new_edge);
    }
    return edges;
}

// Returns the set of actually deleted node ids.
std::vector<uint32_t> DeleteNodesGetOrphanEdges(const std::vector<uint32_t>& node_ids, GraphState& state, std::set<uint32_t>& orphan_edges) {
    std::vector<uint32_t> deleted_node_ids;
    for (const uint32_t node_id : node_ids) {
        const auto iter = state.nodes_map.find(node_id);
        if (iter == state.nodes_map.end()) continue;
        // TODO: Put the node's adjacent edges in to 'orphan_edges'.
        state.nodes_map.erase(iter);
        deleted_node_ids.push_back(node_id);
    }
    return deleted_node_ids;
}

// Returns the collection of actually deleted edges.
std::map<uint32_t, data::GraphEdge> DeleteEdges(const std::set<uint32_t>& edge_ids, GraphState& state) {
    std::map<uint32_t, data::GraphEdge> deleted_edges;
    for (const uint32_t edge_id : edge_ids) {
        const auto iter = state.edges_map.find(edge_id);
        if (iter == state.edges_map.end()) continue;
        // TODO: Queue any post-deletion action following the deletion of this edge.
        deleted_edges[edge_id] = std::move(iter->second);
        state.edges_map.erase(iter);
    }
    return deleted_edges;
}

}  // namespace

GraphEngineImpl::GraphEngineImpl() {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
    .edgeid_splitmix_offset = 412234ULL,
  };
}

std::vector<data::GraphNode> GraphEngineImpl::GetNodes() const {
  std::vector<data::GraphNode> result;
  result.reserve(state_.nodes_map.size());
  for (const auto& [_, node] : state_.nodes_map) {
    result.push_back(node);
  }
  return result;
}

std::vector<data::GraphEdge> GraphEngineImpl::GetEdges() const {
  std::vector<data::GraphEdge> result;
  result.reserve(state_.edges_map.size());
  for (const auto& [_, edge] : state_.edges_map) {
    result.push_back(edge);
  }
  return result;
}

std::vector<data::GraphSlot> GraphEngineImpl::GetSlots() const {
  std::vector<data::GraphSlot> result;
  result.reserve(state_.slots.size());
  for (const auto& [_, slot] : state_.slots) {
    result.push_back(slot);
  }
  return result;
}

absl::StatusOr<data::GraphNode> GraphEngineImpl::InsertNode(const data::FunctionInfo& fn_info) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };

  auto node_or = CreateNode(ctx, fn_info);
  if (!node_or.ok()) return std::move(node_or).status();

  return node_or;
}


absl::StatusOr<std::vector<data::GraphEdge>> GraphEngineImpl::AddEdges(const std::vector<data::AddEdgeEntry>& entries) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };
  return AddEdgesInternal(ctx, entries);
  // result.nodes_added = std::move(add_result.nodes_added);
  // result.edges_added = std::move(add_result.edges_added);
  // AddAndResetTopoOrder(result);
}

absl::StatusOr<data::NodeAndEdgeIds> GraphEngineImpl::DeleteElements(const data::NodeAndEdgeIds& input_ids) {
  std::set<uint32_t> unique_edge_ids(input_ids.edge_ids.begin(), input_ids.edge_ids.end());
  const std::vector<uint32_t> deleted_node_ids = DeleteNodesGetOrphanEdges(input_ids.node_ids, state_, unique_edge_ids);

  std::map<uint32_t, data::GraphEdge> deleted_edges = DeleteEdges(unique_edge_ids, state_);
  for (const uint32_t node_id : deleted_node_ids) {
    // toposort_order->RemoveNode(node_id);
  }
  std::vector<uint32_t> deleted_edge_ids;
  deleted_edge_ids.reserve(deleted_edges.size());
  for (const auto& [edge_id, edge] : deleted_edges) {
    deleted_edge_ids.push_back(edge_id);
    // toposort_order->RemoveEdge(edge.srcNode, edge.destNode);
  }
  return data::NodeAndEdgeIds {
    .node_ids = std::move(deleted_node_ids),
    .edge_ids = std::move(deleted_edge_ids),
  };
}

absl::StatusOr<data::NodeAndEdgeIds> GraphEngineImpl::ClearGraph() {
  std::vector<uint32_t> deleted_node_ids;
  std::vector<uint32_t> deleted_edge_ids;

  // Remove the edges and collect their ids.
  {
    decltype(state_.edges_map) edges_map;
    edges_map.swap(state_.edges_map);
    deleted_edge_ids.reserve(edges_map.size());
    for (const auto& [id, _] : edges_map) {
      deleted_edge_ids.push_back(id);
    }
  }
  // Remove the nodes and collect their ids.
  {
    decltype(state_.nodes_map) nodes_map;
    nodes_map.swap(state_.nodes_map);
    deleted_node_ids.reserve(nodes_map.size());
    for (const auto& [id, _] : nodes_map) {
      deleted_node_ids.push_back(id);
    }
  }

  // Remove the slots and other collections.
  state_.slots.clear();
  state_.node_id_lookup.clear();

  return data::NodeAndEdgeIds {
    .node_ids = std::move(deleted_node_ids),
    .edge_ids = std::move(deleted_edge_ids),
  };
}

void GraphEngineImpl::AddAndResetTopoOrder(EngineOpResult& result) {
  if (topo_sort_order_.HasDirtyBitSet()) {
    result.topo_order = topo_sort_order_.CurrentOrder();
    topo_sort_order_.ClearDirtyBit();
  }
}

}  // namespace ujcore
