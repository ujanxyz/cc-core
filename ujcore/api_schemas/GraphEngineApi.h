#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"

namespace ujcore {

struct GraphEngineApi {
    // API: getGraph
    struct GetGraphResponse {
        std::vector<plinfo::NodeInfo> nodeInfos;
        std::vector<plinfo::EdgeInfo> edgeInfos;
        std::vector<plinfo::SlotInfo> slotInfos;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfos, edgeInfos, slotInfos);
    };

    // API: createNode
    struct CreateNodeRequest {
        data::FunctionInfo func;
        DEFINE_STRUCT_VISITOR_FUNCTION(func);
    };
    struct CreateNodeResponse {
        std::optional<plinfo::NodeInfo> nodeInfo;
        std::optional<plstate::NodeState> nodeState;

        std::vector<plinfo::SlotInfo> ins;
        std::vector<plinfo::SlotInfo> outs;
        std::vector<plinfo::SlotInfo> inouts;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfo, nodeState, ins, outs, inouts);
    };

    // API: addEdge
    struct AddEdgeRequest {
        std::string sourceNode;
        std::string sourceSlot;
        std::string targetNode;
        std::string targetSlot;
        DEFINE_STRUCT_VISITOR_FUNCTION(sourceNode, sourceSlot, targetNode, targetSlot);
    };
    struct AddEdgeResponse {
        std::optional<plinfo::EdgeInfo> edgeInfo;
        DEFINE_STRUCT_VISITOR_FUNCTION(edgeInfo);
    };

    // API: deleteElements
    struct DeleteElementsRequest {
        std::vector<std::string> nodeIds;
        std::vector<std::string> edgeIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds);
    };
    struct DeleteElementsResponse {
        std::vector<std::string> nodeIds;
        std::vector<std::string> edgeIds;
        std::vector<std::string> topoOrder;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds, topoOrder);
    };

    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<AddEdgeRequest, AddEdgeResponse> addEdge;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<VoidType, VoidType> clearGraph;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        addEdge,
        deleteElements,
        clearGraph);
};


}  // namespace ujcore
