#include "ujcore/graph/GraphEngine.hpp"

namespace ujcore {

GraphEngine::GraphEngine() {
}

std::string GraphEngine::InsertNewNode(const std::string& fn_spec) {
    NodeData node = {
        .func_uri = "/fn/points-on-curve",
        .label = u8"Point on Curve (2)",
        .slot_ids = {"s1", "s2"},
    };

    std::string node_id = id_gen_.GetNextId();
    state_.nodes[node_id] = node;
    return node_id;
}

std::tuple<int32_t, int32_t, int32_t>
  GraphEngine::GetElementCounts() const {
  int32_t count_nodes = state_.nodes.size();
  int32_t count_edges = state_.edges.size();
  int32_t count_slots = state_.slots.size();
  return std::make_tuple(count_nodes, count_edges, count_slots);
}

}  // namespace ujcore
