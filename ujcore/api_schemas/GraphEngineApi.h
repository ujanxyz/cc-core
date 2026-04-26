#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/FunctionInfo.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/ResourceInfo.h"
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

    // API: getNodeStates
    struct GetNodeStatesRequest {
        std::vector<NodeId> nodeIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds);
    };
    struct GetNodeStatesResponse {
        std::vector<std::pair<NodeId, plstate::NodeState>> nodeStates;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeStates);
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

    // API: setEncodedData
    struct SetEncodedDataRequest {
        // If set the data is for a input node, else it is for a slot.
        bool isNode = false;

        // Either node id or slot id should be set, depending on the value of `isNode`.
        std::optional<NodeId> nodeId;
        std::optional<SlotId> slotId;

        // If this is not set, the existing manual data will be cleared.
        std::optional<plstate::EncodedData> encodedData;

        DEFINE_STRUCT_VISITOR_FUNCTION(isNode, nodeId, slotId, encodedData);
    };

    // API: runPipeline
    struct RunPipelineRequest {
        // If true, the pipeline will be built.
        bool build = false;

        // If true, the pipeline will be executed.
        bool execute = false;

        DEFINE_STRUCT_VISITOR_FUNCTION(build, execute);
    };
    struct RunPipelineResponse {
        std::vector<plstate::GraphRunOutput> outputs;

        DEFINE_STRUCT_VISITOR_FUNCTION(outputs);
    };

    // API: getResources
    struct GetResourcesResponse {
        std::vector<ResourceInfo> resources;

        DEFINE_STRUCT_VISITOR_FUNCTION(resources);
    };

    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<CreateIONodeRequest, CreateIONodeResponse> createIONode;
    cppschema::ApiStub<AddEdgeRequest, AddEdgeResponse> addEdge;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<GetNodeStatesRequest, GetNodeStatesResponse> getNodeStates;
    cppschema::ApiStub<GetSlotStatesRequest, GetSlotStatesResponse> getSlotStates;
    cppschema::ApiStub<VoidType, VoidType> clearGraph;
    cppschema::ApiStub<VoidType, GetAvailableFuncsResponse> getAvailableFuncs;
    cppschema::ApiStub<SetEncodedDataRequest, VoidType> setEncodedData;
    cppschema::ApiStub<RunPipelineRequest, RunPipelineResponse> runPipeline;
    cppschema::ApiStub<VoidType, GetResourcesResponse> getResources;

    DEFINE_API_VISITOR_FUNCTION(
        getGraph,
        createNode,
        createIONode,
        addEdge,
        deleteElements,
        getNodeStates,
        getSlotStates,
        clearGraph,
        getAvailableFuncs,
        setEncodedData,
        runPipeline,
        getResources);
};

}  // namespace ujcore
