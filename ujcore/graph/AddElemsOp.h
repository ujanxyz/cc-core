#pragma once

#include <set>
#include <string>
#include <vector>

#include "ujcore/graph/GraphOpsContext.h"
#include "ujcore/functions/NodeFunctionSpec.hpp"

namespace ujcore {

/**
 * For a new node based on a function spec, construct its slots.
 * Each node type defines its slot topology and naming convention:
 *
 * - Graph Input ("in")
 *   - 1 output slot
 *   - "<id>$out"
 *
 * - Graph Output ("out")
 *   - 1 input slot
 *   - "<id>$in"
 *
 * - Function Node ("fn")
 *   - N input slots:  "<id>$in:<param-name>"
 *   - M output slots: "<id>$out:<param-name>"
 *
 * - Operator Node ("op")
 *   - 1 implicit input  slot: "<id>$in"
 *   - 1 implicit output slot: "<id>$out"
 *   - N named input slots:    "<id>$in:<param-name>"
 *
 * - Lambda Node ("lam")
 *   - 1 output slot (functor): "<id>$out"
 *   - N input slots:           "<id>$in:<param-name>"
 */
void CreateNodeSlots(
    const NodeFunctionSpec& func_spec,
    std::string_view node_id,
    std::vector<SlotData>& output);

struct AddElemsResult {
  std::set<std::string> nodes_added;
  std::set<std::string> edges_added;
};

void AddElemsOp_Nodes(GraphOpsContext& ctx, const std::vector<NodeFunctionSpec>& func_specs, AddElemsResult& result);

void AddElemsOp_Edges(GraphOpsContext& ctx, const std::vector<EdgeData>& edges, AddElemsResult& result);


}  // namespace ujcore
