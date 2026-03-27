#pragma once

#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "ujcore/data/graph/ClientMessages.h"
#include "ujcore/graph/GraphOpsContext.h"
#include "ujcore/functions/NodeFunctionSpec.hpp"

namespace ujcore {

absl::StatusOr<data::GraphNode> AddElemsOp_CreateNode(GraphOpsContext& ctx, const NodeFunctionSpec& fnspec);

absl::StatusOr<std::vector<data::GraphEdge>> AddElemsOp_AddEdges(GraphOpsContext& ctx, const std::vector<data::AddEdgeEntry>& entries);

}  // namespace ujcore
