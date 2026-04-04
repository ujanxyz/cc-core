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
        uint32_t sourceNode;
        std::string sourceSlot;
        uint32_t targetNode;
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
        std::vector<uint32_t> nodeIds;
        std::vector<uint32_t> edgeIds;
        std::vector<plinfo::SlotId> deletedSlotIds;
        std::vector<plinfo::SlotId> affectedSlotIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds);
    };
    struct DeleteElementsResponse {
        std::vector<uint32_t> nodeIds;
        std::vector<uint32_t> edgeIds;
        std::vector<plinfo::SlotId> deletedSlotIds;
        std::vector<plinfo::SlotId> affectedSlotIds;
        std::vector<std::string> topoOrder;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds, edgeIds, deletedSlotIds, affectedSlotIds, topoOrder);
    };

    // API: getSlotStates
   struct GetSlotStatesRequest {
        std::vector<plinfo::SlotId> slotIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotIds);
    };
    struct GetSlotStatesResponse {
        std::vector<std::pair<plinfo::SlotId, plstate::SlotState>> slotStates;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotStates);
    };

    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<AddEdgeRequest, AddEdgeResponse> addEdge;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<GetSlotStatesRequest, GetSlotStatesResponse> getSlotStates;        
    cppschema::ApiStub<VoidType, VoidType> clearGraph;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        addEdge,
        deleteElements,
        getSlotStates,
        clearGraph);
};


}  // namespace ujcore
