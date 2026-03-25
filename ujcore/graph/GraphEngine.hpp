#pragma once

#include <set>
#include <string>
#include <tuple>

#include "ujcore/graph/GraphState.hpp"
#include "ujcore/functions/NodeFunctionSpec.hpp"

namespace ujcore {

struct EngineOpResult {
  std::set<std::string> nodes_added;
  std::set<std::string> edges_added;
  std::set<std::string> nodes_deleted;
  std::set<std::string> edges_deleted;
};

class GraphEngine {
 public:
  struct DeleteRequest {
    std::set<std::string> node_ids;
    std::set<std::string> edge_ids;
  };

  GraphEngine();

  GraphState* mutable_state() {
    return &state_;
  }

  std::tuple<int32_t, int32_t, int32_t>
  GetElementCounts() const;

  void AddElements(const std::vector<NodeFunctionSpec>& func_specs, EngineOpResult& result);
  void DeleteElements(const std::set<std::string>& node_ids, const std::set<std::string>& edge_ids, EngineOpResult& result);

 private:
  GraphConfig config_;
  GraphState state_;
};

}  // namespace ujcore
