#pragma once

#include <string>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"

namespace ujcore {

struct GraphEngineApi {
    struct ElementStats {
        int32_t num_nodes {0};
        int32_t num_edges {0};
        int32_t num_slots {0};
        DEFINE_STRUCT_VISITOR_FUNCTION(num_nodes, num_edges, num_slots);
    };

    cppschema::ApiStub<std::string, std::string> addNode;
    cppschema::ApiStub<VoidType, ElementStats> getElemStats;

    cppschema::ApiStub<VoidType, VoidType> clearGraph;

    DEFINE_API_VISITOR_FUNCTION(
        addNode,
        getElemStats,
        clearGraph);
};


}  // namespace ujcore
