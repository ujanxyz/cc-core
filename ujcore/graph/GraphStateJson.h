#pragma once

#include <string>

#include "absl/status/statusor.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/TopoSortState.h"

#include "nlohmann/json.hpp"

namespace ujcore {

// Utility functions to encode graph state structure to JSON string.
absl::StatusOr<std::string> EncodeGraphState(const GraphState& state);

// Utility functions to decode graph state from JSON string.
absl::StatusOr<GraphState> DecodeGraphState(const std::string& encoded);

} // namespace ujcore
