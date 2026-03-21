#pragma once

#include <string>

#include "ujcore/graph/ops/GraphOperatorBase.hpp"

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

class InsertNodeOp : public GraphOperatorBase {
 public:
  std::string apply(std::string payload_str, GraphState& state) override;
};

}  // namespace ujcore