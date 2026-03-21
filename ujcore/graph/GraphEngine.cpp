#include "ujcore/graph/GraphEngine.hpp"

namespace ujcore {

GraphEngine::GraphEngine() {
}

std::tuple<int32_t, int32_t, int32_t>
  GraphEngine::GetElementCounts() const {
  int32_t count_nodes = state_.nodes.size();
  int32_t count_edges = state_.edges.size();
  int32_t count_slots = state_.slots.size();
  return std::make_tuple(count_nodes, count_edges, count_slots);
}

}  // namespace ujcore
