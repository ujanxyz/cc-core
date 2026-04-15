#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/FunctionInfo.h"
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
        FunctionInfo func;
        DEFINE_STRUCT_VISITOR_FUNCTION(func);
    };
    struct CreateNodeResponse {
        std::optional<plinfo::NodeInfo> nodeInfo;
        std::optional<plstate::NodeState> nodeState;

        std::vector<plinfo::SlotInfo> inInfos;
        std::vector<plinfo::SlotInfo> outInfos;
        std::vector<plinfo::SlotInfo> inoutInfos;

        std::vector<plstate::SlotState> inStates;
        std::vector<plstate::SlotState> outStates;
        std::vector<plstate::SlotState> inoutStates;

        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfo, nodeState, inInfos, outInfos, inoutInfos, inStates, outStates, inoutStates);
    };

    // API: addEdge
    struct AddEdgeRequest {
        NodeId sourceNode;
        std::string sourceSlot;
        NodeId targetNode;
        std::string targetSlot;
        DEFINE_STRUCT_VISITOR_FUNCTION(sourceNode, sourceSlot, targetNode, targetSlot);
    };
    struct AddEdgeResponse {
        std::optional<plinfo::EdgeInfo> edgeInfo;
        std::optional<plstate::SlotState> sourceState;
        std::optional<plstate::SlotState> targetState;
        DEFINE_STRUCT_VISITOR_FUNCTION(edgeInfo, sourceState, targetState);
    };

    // API: deleteElements
    struct DeleteElementsRequest {
        std::vector<NodeId> nodeIds;
        std::vector<EdgeId> edgeIds;
        std::vector<SlotId> deletedSlotIds;
        std::vector<SlotId> affectedSlotIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds);
    };
    struct DeleteElementsResponse {
        std::vector<NodeId> nodeIds;
        std::vector<EdgeId> edgeIds;
        std::vector<SlotId> deletedSlotIds;
        std::vector<SlotId> affectedSlotIds;
        std::vector<std::string> topoOrder;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds, deletedSlotIds, affectedSlotIds, topoOrder);
    };

    // API: getSlotStates
   struct GetSlotStatesRequest {
        std::vector<SlotId> slotIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotIds);
    };
    struct GetSlotStatesResponse {
        std::vector<std::pair<SlotId, plstate::SlotState>> slotStates;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotStates);
    };

    // API: getAvailableFuncs
    struct GetAvailableFuncsResponse {
        std::vector<FunctionInfo> infos;
        DEFINE_STRUCT_VISITOR_FUNCTION(infos);
    };

    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<AddEdgeRequest, AddEdgeResponse> addEdge;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<GetSlotStatesRequest, GetSlotStatesResponse> getSlotStates;        
    cppschema::ApiStub<VoidType, VoidType> clearGraph;
    cppschema::ApiStub<VoidType, GetAvailableFuncsResponse> getAvailableFuncs;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        addEdge,
        deleteElements,
        getSlotStates,
        clearGraph,
        getAvailableFuncs);
};

}  // namespace ujcore
