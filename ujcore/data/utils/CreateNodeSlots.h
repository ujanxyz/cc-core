#pragma once

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/graph/GraphSlot.h"

namespace ujcore::data {

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
absl::StatusOr<std::vector<GraphSlot>>
CreateNodeSlots(const FunctionInfo& fn_info, const uint32_t node_id, const std::string& node_alnumid);

}  // namespace ujcore::data
