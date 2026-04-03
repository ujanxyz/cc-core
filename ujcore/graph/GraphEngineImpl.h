#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/plinfo.h"
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

class GraphEngineImpl {
 public:
  struct DeleteRequest {
    std::set<std::string> node_ids;
    std::set<std::string> edge_ids;
  };

  GraphEngineImpl();

  GraphState* mutable_state() {
    return &state_;
  }

  std::vector<plinfo::NodeInfo> GetNodeInfos() const;
  std::vector<plinfo::EdgeInfo> GetEdgeInfos() const;
  std::vector<plinfo::SlotInfo> GetSlotInfos() const;

  // Upon success returns the same number of SlotInfos
  absl::StatusOr<std::vector<plinfo::SlotInfo>> LookupNodeSlots(uint32_t nodeId, const std::vector<std::string>& slotNames) const;


  absl::StatusOr<plinfo::NodeInfo> AddNode(const data::FunctionInfo& fn_info);
  absl::StatusOr<plinfo::EdgeInfo> AddEdge(
      const std::string& sourceNode, const std::string& sourceSlot,
      const std::string& targetNode, const std::string& targetSlot);

  absl::StatusOr<std::vector<std::string> /* edge ids */>
  DeleteElements(const std::vector<std::string>& nodeIds, const std::vector<std::string>& edgeIds);

  // Returns the total number of deleted nodes + edges.
  absl::StatusOr<int> ClearGraph();

 private:
  void AddAndResetTopoOrder(EngineOpResult& result);

  GraphConfig config_;
  GraphState state_;
  TopoSortOrder topo_sort_order_;
};

}  // namespace ujcore

