#pragma once

#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/graph/FunctionInfo.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/TopoSorter.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphTypes.h"

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
  struct AddEdgeEntry {
    NodeId node0 {0};
    std::string slot0;
    NodeId node1 {0};
    std::string slot1;
    std::optional<EdgeId> overrideEdgeId;
  };

  struct DeleteRequest {
    std::set<std::string> node_ids;
    std::set<std::string> edge_ids;
  };

  GraphBuilder(GraphState& state, TopoSorter& topoSorter);

  GraphState* mutable_state() {
    return &state_;
  }

  // Upon success returns the same number of SlotInfos
  absl::StatusOr<std::vector<grph::SlotInfo>> LookupNodeSlotInfos(NodeId nodeId, const std::vector<std::string>& slotNames) const;

  absl::StatusOr<grph::NodeInfo> AddFuncNode(const FunctionInfo& fnInfo, std::optional<NodeId> overrideId);

  absl::StatusOr<std::tuple<grph::NodeInfo, grph::SlotInfo>> AddIONode(const std::string& dtype, bool isOutput, std::optional<NodeId> overrideId);

  absl::StatusOr<std::vector<grph::EdgeInfo>> AddEdges(
      const std::vector<GraphBuilder::AddEdgeEntry>& entries);

  absl::StatusOr<std::tuple<std::vector<EdgeId> /* edge ids */, std::set<SlotId> /* deleted slot ids*/, std::set<SlotId> /* affected slot ids */ >>
  DeleteElements(const std::vector<NodeId>& nodeIds, const std::vector<EdgeId>& edgeIds);

  absl::Status SetNodeEncodedData(const NodeId nodeId, const std::optional<grph::EncodedData>& encodedData);
  /// @deprecated Use SetGraphInputs instead, which now operates at the slot level.
  /// This method is deprecated because graph input nodes should have their encoded data
  /// stored at the slot level (the "$out" slot) instead of the node level.
  absl::Status SetSlotEncodedData(const SlotId slotId, const std::optional<grph::EncodedData>& encodedData);

  // Sets the graph input data for the given input nodes. The input data should be encoded as strings.
  absl::Status SetGraphInputs(const std::vector<std::tuple<NodeId, std::string /* encoded data */>>& graphInputs);

  // Clears the graph input data for the given input nodes.
  absl::Status ClearGraphInputs(const std::vector<NodeId>& nodeIds);


  // Returns the total number of deleted nodes + edges.
  absl::StatusOr<int> ClearGraph();

  // Validates whether an edge can be added between the given source and target slots, and returns the validation result.
  absl::StatusOr<grph::SlotState::Validity> ValidateEdge(const SlotId sourceSlotId, const SlotId targetSlotId) const;

private:
  GraphState& state_;
  TopoSorter& topoSorter_;

  GraphConfig config_;
};

}  // namespace ujcore
