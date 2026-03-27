#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cppschema/common/visitor_macros.h"

namespace ujcore::data {

struct GraphNode {
    // Valid node id should start from 1, i.e. always > 0.
    uint32_t id {0};

    // Base-62 alphanumeric id, e.g. "ZBqg1rBrgq". This is directly translated from the int64 id.
    std::string alnumid;

    // UI visible label.
    // TODO: Enable u8 string in JS conversion.
    // std::u8string label;

    // Identifies the node function used, e.g. "/fn/points-on-curve"
    std::string fnuri;

    // names of the slots contained in this node.
    std::vector<std::string> slots;

    DEFINE_STRUCT_VISITOR_FUNCTION(id, alnumid, fnuri, slots);
};

}  // namespace ujcore::data