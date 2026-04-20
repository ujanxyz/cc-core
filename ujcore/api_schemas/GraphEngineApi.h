#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/FunctionInfo.h"
#include "ujcore/data/IdTypes.h"
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

    // API: createIONode
    struct CreateIONodeRequest {
        std::string dtype;
        bool isOutput = false;  // if true, it's an output node.
        DEFINE_STRUCT_VISITOR_FUNCTION(dtype, isOutput);
    };
    struct CreateIONodeResponse {
        std::optional<plinfo::NodeInfo> nodeInfo;
        std::optional<plstate::NodeState> nodeState;
        std::optional<plinfo::SlotInfo> slotInfo;
        std::optional<plstate::SlotState> slotState;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfo, nodeState, slotInfo, slotState);
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

    // API: syncEncodedData
    struct SyncEncodedDataRequest {
        // In the following `SlotId` entries, the slot name can be empty, which indicates
        // that the manual data applies to the entire node rather than a specific slot.
        std::map<SlotId, std::string /* encoded data */> updateIds;

        // Delete IDs take precedence over update IDs. If an ID appears in both, it will
        // be deleted and the update will be ignored.
        std::vector<SlotId> deleteIds;

        // Fetch the current data for these IDs after the run.
        // Mutations and deletions are applied
        std::vector<SlotId> fetchIds;

        DEFINE_STRUCT_VISITOR_FUNCTION(updateIds, deleteIds, fetchIds);
    };
    struct SyncEncodedDataResponse {
        std::map<SlotId, std::optional<std::string /* encoded data */>> manualData;

        DEFINE_STRUCT_VISITOR_FUNCTION(manualData);
    };

    // API: syncGraphInputs
    struct SyncGraphInputsRequest {
        std::map<NodeId, std::string /* encoded data */> updateIds;
        std::vector<NodeId> deleteIds;

        DEFINE_STRUCT_VISITOR_FUNCTION(updateIds, deleteIds);
    };
    struct SyncGraphInputsResponse {
        std::map<NodeId, std::optional<std::string /* encoded data */>> inputData;

        DEFINE_STRUCT_VISITOR_FUNCTION(inputData);
    };

    // API: runPipeline
    struct RunPipelineResponse {
        std::optional<plstate::GraphRunResult> runResult;

        DEFINE_STRUCT_VISITOR_FUNCTION(runResult);
    };

    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<CreateIONodeRequest, CreateIONodeResponse> createIONode;
    cppschema::ApiStub<AddEdgeRequest, AddEdgeResponse> addEdge;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<GetSlotStatesRequest, GetSlotStatesResponse> getSlotStates;        
    cppschema::ApiStub<VoidType, VoidType> clearGraph;
    cppschema::ApiStub<VoidType, GetAvailableFuncsResponse> getAvailableFuncs;
    cppschema::ApiStub<SyncEncodedDataRequest, SyncEncodedDataResponse> syncEncodedData;
    cppschema::ApiStub<SyncGraphInputsRequest, SyncGraphInputsResponse> syncGraphInputs;
    cppschema::ApiStub<VoidType, RunPipelineResponse> runPipeline;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        createIONode,
        addEdge,
        deleteElements,
        getSlotStates,
        clearGraph,
        getAvailableFuncs,
        syncEncodedData,
        syncGraphInputs,
        runPipeline);
};

}  // namespace ujcore
