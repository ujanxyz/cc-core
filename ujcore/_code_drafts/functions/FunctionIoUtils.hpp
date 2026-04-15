#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"
#include "ujcore/functions/NodeFunctionSpec.hpp"

namespace ujcore {

// Convert the spec struct to JSON string using nlohmann json.
// TODO: Add nicer doc.
std::string NodeFuncSpecToJsonStr(const NodeFunctionSpec& spec);

// Parse a JSON string to node spec.
bool ParseNodeFuncSpecFromJsonStr(std::string_view content, NodeFunctionSpec& spec);

// Parse a JSON string to node spec.
bool ParseNodeFuncSpecFromJsonObj(const nlohmann::json& j, NodeFunctionSpec& spec);

}  // namespace ujcore
