#include "ujcore/graph/GraphEngine.hpp"

#include "absl/status/status.h"
#include "ujcore/graph/AddElemsOp.h"
#include "ujcore/graph/GraphOpsContext.h"

namespace ujcore {
namespace {

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

GraphEngine::GraphEngine() {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
    .edgeid_splitmix_offset = 412234ULL,
  };
}

std::vector<data::GraphNode> GraphEngine::GetNodes() const {
  std::vector<data::GraphNode> result;
  result.reserve(state_.nodes_map.size());
  for (const auto& [_, node] : state_.nodes_map) {
    result.push_back(node);
  }
  return result;
}

std::vector<data::GraphEdge> GraphEngine::GetEdges() const {
  std::vector<data::GraphEdge> result;
  result.reserve(state_.edges_map.size());
  for (const auto& [_, edge] : state_.edges_map) {
    result.push_back(edge);
  }
  return result;
}

std::vector<data::GraphSlot> GraphEngine::GetSlots() const {
  std::vector<data::GraphSlot> result;
  result.reserve(state_.slots.size());
  for (const auto& [_, slot] : state_.slots) {
    result.push_back(slot);
  }
  return result;
}

absl::StatusOr<data::GraphNode> GraphEngine::InsertNode(const data::FunctionInfo& fn_info) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };

  auto node_or = AddElemsOp_CreateNode(ctx, fn_info);
  if (!node_or.ok()) return std::move(node_or).status();

  return node_or;
}


absl::StatusOr<std::vector<data::GraphEdge>> GraphEngine::AddEdges(const std::vector<data::AddEdgeEntry>& entries) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };
  return AddElemsOp_AddEdges(ctx, entries);
  // result.nodes_added = std::move(add_result.nodes_added);
  // result.edges_added = std::move(add_result.edges_added);
  // AddAndResetTopoOrder(result);
}

absl::StatusOr<data::NodeAndEdgeIds> GraphEngine::DeleteElements(const data::NodeAndEdgeIds& input_ids) {
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

absl::StatusOr<data::NodeAndEdgeIds> GraphEngine::ClearGraph() {
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

void GraphEngine::AddAndResetTopoOrder(EngineOpResult& result) {
  if (topo_sort_order_.HasDirtyBitSet()) {
    result.topo_order = topo_sort_order_.CurrentOrder();
    topo_sort_order_.ClearDirtyBit();
  }
}

}  // namespace ujcore
