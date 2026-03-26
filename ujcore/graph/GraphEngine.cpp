#include "ujcore/graph/GraphEngine.hpp"

#include "ujcore/graph/AddElemsOp.h"
#include "ujcore/graph/DeleteElementsOp.h"
#include "ujcore/graph/GraphOpsContext.h"

namespace ujcore {

GraphEngine::GraphEngine() {
  config_ = {
    .nodeid_splitmix_offset = 0ULL,
    .edgeid_splitmix_offset = 412234ULL,
    .slotid_splitmix_offset = 987123ULL,
  };
}

std::tuple<int32_t, int32_t, int32_t>
  GraphEngine::GetElementCounts() const {
  int32_t count_nodes = state_.nodes.size();
  int32_t count_edges = state_.edges.size();
  int32_t count_slots = state_.slots.size();
  return std::make_tuple(count_nodes, count_edges, count_slots);
}

void GraphEngine::AddElements(const std::vector<NodeFunctionSpec>& func_specs, EngineOpResult& result) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };
  AddElemsResult add_result;
  AddElemsOp_Nodes(ctx, func_specs, add_result);
  result.nodes_added = std::move(add_result.nodes_added);
  result.edges_added = std::move(add_result.edges_added);
  AddAndResetTopoOrder(result);
}

void GraphEngine::AddEdgeConnections(const std::vector<EdgeData>& edges, EngineOpResult& result) {
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };
  AddElemsResult add_result;
  AddElemsOp_Edges(ctx, edges, add_result);
  result.nodes_added = std::move(add_result.nodes_added);
  result.edges_added = std::move(add_result.edges_added);
  AddAndResetTopoOrder(result);
}

void GraphEngine::DeleteElements(const std::set<std::string>& node_ids, const std::set<std::string>& edge_ids, EngineOpResult& result) {
  DeleteElementsOpMutableIds elem_ids = {
    .node_ids = node_ids,
    .edge_ids = edge_ids,
  };
  GraphOpsContext ctx = {
    .config = config_,
    .state = state_,
    .toposort_order = &topo_sort_order_,
  };
  DoDeleteElemsOp(ctx, elem_ids);
  result.nodes_deleted = std::move(elem_ids.node_ids);
  result.edges_deleted = std::move(elem_ids.edge_ids);
  AddAndResetTopoOrder(result);
}

void GraphEngine::AddAndResetTopoOrder(EngineOpResult& result) {
  if (topo_sort_order_.HasDirtyBitSet()) {
    result.topo_order = topo_sort_order_.CurrentOrder();
    topo_sort_order_.ClearDirtyBit();
  }
}

}  // namespace ujcore
