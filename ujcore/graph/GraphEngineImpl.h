#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/graph/GraphState.h"
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
  absl::StatusOr<std::vector<plinfo::SlotInfo>> LookupNodeSlotInfos(uint32_t nodeId, const std::vector<std::string>& slotNames) const;

  // Upon success returns the same number of SlotStates
  absl::StatusOr<std::vector<std::pair<plinfo::SlotId, plstate::SlotState>>>
  LookupSlotStates(const std::vector<plinfo::SlotId>& slotIds);


  absl::StatusOr<plinfo::NodeInfo> AddNode(const data::FunctionInfo& fn_info);
  absl::StatusOr<plinfo::EdgeInfo> AddEdge(
      const uint32_t sourceNode, const std::string& sourceSlot,
      const uint32_t targetNode, const std::string& targetSlot);

  absl::StatusOr<std::tuple<std::vector<uint32_t> /* edge ids */, std::set<plinfo::SlotId> /* deleted slot ids*/, std::set<plinfo::SlotId> /* affected slot ids */ >>
  DeleteElements(const std::vector<uint32_t>& nodeIds, const std::vector<uint32_t>& edgeIds);

  // Returns the total number of deleted nodes + edges.
  absl::StatusOr<int> ClearGraph();

 private:
  void AddAndResetTopoOrder(EngineOpResult& result);

  GraphConfig config_;
  GraphState state_;
  TopoSortOrder topo_sort_order_;
};

}  // namespace ujcore

