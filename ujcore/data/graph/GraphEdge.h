#pragma once

#include <cstdint>
#include <string>

#include "cppschema/common/visitor_macros.h"

namespace ujcore::data {

struct GraphEdge {
    // Valid node id should start from 1, i.e. always > 0.
    uint32_t id {0};

    // Id (raw) of the source and target node.
    uint32_t node0 {0};
    uint32_t node1 {0};

    std::string slot0;
    std::string slot1;

    DEFINE_STRUCT_VISITOR_FUNCTION(id, node0, node1, slot0, slot1);
};

}  // namespace ujcore::data
