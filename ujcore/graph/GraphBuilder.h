#pragma once

#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/data/FunctionInfo.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/TopoSortOrder.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/FunctionSpec.h"

namespace ujcore {

struct EngineOpResult {
  std::set<std::string> nodes_added;
  std::set<std::string> edges_added;
  std::set<std::string> nodes_deleted;
  std::set<std::string> edges_deleted;
  std::optional<std::vector<std::string>> topo_order;
};

class GraphBuilder {
 public:
  struct DeleteRequest {
    std::set<std::string> node_ids;
    std::set<std::string> edge_ids;
  };

  GraphBuilder(GraphState& state, TopoSortOrder& topoSorter);

  GraphState* mutable_state() {
    return &state_;
  }

  // Upon success returns the same number of SlotInfos
  absl::StatusOr<std::vector<plinfo::SlotInfo>> LookupNodeSlotInfos(NodeId nodeId, const std::vector<std::string>& slotNames) const;

  absl::StatusOr<plinfo::NodeInfo> AddFuncNode(const FunctionInfo& fnInfo);

  absl::StatusOr<std::tuple<plinfo::NodeInfo, plinfo::SlotInfo>> AddIONode(const std::string& dtype, bool isOutput);

  absl::StatusOr<plinfo::EdgeInfo> AddEdge(
      const NodeId sourceNode, const std::string& sourceSlot,
      const NodeId targetNode, const std::string& targetSlot);

  absl::StatusOr<std::tuple<std::vector<EdgeId> /* edge ids */, std::set<SlotId> /* deleted slot ids*/, std::set<SlotId> /* affected slot ids */ >>
  DeleteElements(const std::vector<NodeId>& nodeIds, const std::vector<EdgeId>& edgeIds);

  absl::Status SetNodeEncodedData(const NodeId nodeId, const std::optional<plstate::EncodedData>& encodedData);
  absl::Status SetSlotEncodedData(const SlotId slotId, const std::optional<plstate::EncodedData>& encodedData);

  // Sets the graph input data for the given input nodes. The input data should be encoded as strings.
  absl::Status SetGraphInputs(const std::vector<std::tuple<NodeId, std::string /* encoded data */>>& graphInputs);

  // Clears the graph input data for the given input nodes.
  absl::Status ClearGraphInputs(const std::vector<NodeId>& nodeIds);


  // Returns the total number of deleted nodes + edges.
  absl::StatusOr<int> ClearGraph();

  // Validates whether an edge can be added between the given source and target slots, and returns the validation result.
  absl::StatusOr<plstate::SlotState::Validity> ValidateEdge(const SlotId sourceSlotId, const SlotId targetSlotId) const;

  absl::StatusOr<std::vector<FunctionInfo>> GetAvailableFuncInfos() const;

 private:
  GraphState& state_;
  TopoSortOrder& topoSorter_;

  GraphConfig config_;
};

}  // namespace ujcore
