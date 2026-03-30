#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/enum_registry.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/graph/ClientMessages.h"
#include "ujcore/data/graph/FunctionInfo.h"
#include "ujcore/data/graph/GraphEdge.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"

namespace ujcore {

struct GraphEngineApi {
    struct CreateNodeRequest {
        data::FunctionInfo func;
        DEFINE_STRUCT_VISITOR_FUNCTION(func);
    };

    struct ElementStats {
        int32_t num_nodes {0};
        int32_t num_edges {0};
        int32_t num_slots {0};
        DEFINE_STRUCT_VISITOR_FUNCTION(num_nodes, num_edges, num_slots);
    };

    struct GraphDataResponse {
        std::vector<data::GraphNode> nodes;
        std::vector<data::GraphEdge> edges;
        std::vector<data::GraphSlot> slots;
        DEFINE_STRUCT_VISITOR_FUNCTION(slots, nodes, edges);
    };

    struct CreateNodeResponse {
        std::optional<data::GraphNode> node;
        std::vector<data::GraphEdge> edges;
        DEFINE_STRUCT_VISITOR_FUNCTION(node, edges);
    };

    struct AddEdgesRequest {
        std::vector<data::AddEdgeEntry> entries;
        DEFINE_STRUCT_VISITOR_FUNCTION(entries);
    };

    struct AddEdgesResponse {
        std::vector<data::GraphEdge> edges;
        DEFINE_STRUCT_VISITOR_FUNCTION(edges);
    };

    cppschema::ApiStub<VoidType, GraphDataResponse> getGraph;

    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;

    cppschema::ApiStub<AddEdgesRequest, AddEdgesResponse> addEdges;

    cppschema::ApiStub<data::NodeAndEdgeIds, data::NodeAndEdgeIds> deleteElements;

    cppschema::ApiStub<VoidType, data::NodeAndEdgeIds> clearGraph;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        addEdges,
        deleteElements,
        clearGraph);
};


}  // namespace ujcore
