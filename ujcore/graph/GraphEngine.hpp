#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/data/graph/ClientMessages.h"
#include "ujcore/data/graph/GraphEdge.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"
#include "ujcore/functions/NodeFunctionSpec.hpp"
#include "ujcore/graph/GraphState.hpp"
#include "ujcore/graph/TopoSortOrder.h"

namespace ujcore {

struct EngineOpResult {
  std::set<std::string> nodes_added;
  std::set<std::string> edges_added;
  std::set<std::string> nodes_deleted;
  std::set<std::string> edges_deleted;
  std::optional<std::vector<std::string>> topo_order;
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

  std::vector<data::GraphNode> GetNodes() const;
  std::vector<data::GraphEdge> GetEdges() const;
  std::vector<data::GraphSlot> GetSlots() const;

  absl::StatusOr<data::GraphNode> InsertNode(const NodeFunctionSpec& spec);

  absl::StatusOr<std::vector<data::GraphEdge>> AddEdges(const std::vector<data::AddEdgeEntry>& entries);

  absl::StatusOr<data::NodeAndEdgeIds> DeleteElements(const data::NodeAndEdgeIds& input);

  absl::StatusOr<data::NodeAndEdgeIds> ClearGraph();

 private:
  void AddAndResetTopoOrder(EngineOpResult& result);

  GraphConfig config_;
  GraphState state_;
  TopoSortOrder topo_sort_order_;
};

}  // namespace ujcore
