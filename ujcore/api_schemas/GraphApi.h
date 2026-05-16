#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/enum_registry.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/graph/AssetInfo.h"
#include "ujcore/graph/FunctionInfo.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/ResourceInfo.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphTypes.h"

namespace ujcore {

struct GraphApi {
    // API: getGraphMeta
    struct GetGraphMetaResponse {
        uint32_t lastNodeId = 0;
        uint32_t lastEdgeId = 0;

        DEFINE_STRUCT_VISITOR_FUNCTION(lastNodeId, lastEdgeId);
    };

    // API: setGraphMeta
    struct SetGraphMetaRequest {
        uint32_t lastNodeId = 0;
        uint32_t lastEdgeId = 0;

        DEFINE_STRUCT_VISITOR_FUNCTION(lastNodeId, lastEdgeId);
    };

    // API: getGraph
    struct GetGraphResponse {
        std::vector<grph::NodeInfo> nodeInfos;
        std::vector<grph::EdgeInfo> edgeInfos;
        std::vector<grph::SlotInfo> slotInfos;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfos, edgeInfos, slotInfos);
    };

    // API: createNode
    struct CreateNodeRequest {
        FunctionInfo func;

        // If provided, the created node will have this id. Otherwise, a new id will be generated.
        // This is used when the graph is being reconstructed from a serialized state, then we
        // want to preserve the original node ids.
        std::optional<std::string> overrideId;

        DEFINE_STRUCT_VISITOR_FUNCTION(func, overrideId);
    };
    struct CreateNodeResponse {
        std::optional<grph::NodeInfo> nodeInfo;
        std::optional<grph::NodeState> nodeState;

        std::vector<grph::SlotInfo> inInfos;
        std::vector<grph::SlotInfo> outInfos;
        std::vector<grph::SlotInfo> inoutInfos;

        std::vector<grph::SlotState> inStates;
        std::vector<grph::SlotState> outStates;
        std::vector<grph::SlotState> inoutStates;

        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfo, nodeState, inInfos, outInfos, inoutInfos, inStates, outStates, inoutStates);
    };

    // API: createIONode
    struct CreateIONodeRequest {
        std::string dtype;
        bool isOutput = false;  // if true, it's an output node.
        // If provided, the created node will have this id. Otherwise, a new id will be generated.
        std::optional<std::string> overrideId;

        DEFINE_STRUCT_VISITOR_FUNCTION(dtype, isOutput, overrideId);
    };
    struct CreateIONodeResponse {
        std::optional<grph::NodeInfo> nodeInfo;
        std::optional<grph::NodeState> nodeState;
        std::optional<grph::SlotInfo> slotInfo;
        std::optional<grph::SlotState> slotState;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeInfo, nodeState, slotInfo, slotState);
    };

    // API: addEdges
    struct AddEdgesRequest {
        struct Entry {
            EncodedSlotId source;
            EncodedSlotId target;

            std::optional<uint32_t> overrideEdgeId;

            DEFINE_STRUCT_VISITOR_FUNCTION(source, target, overrideEdgeId);
        };
        std::vector<Entry> entries;

        DEFINE_STRUCT_VISITOR_FUNCTION(entries);
    };
    struct AddEdgesResponse {
        std::vector<grph::EdgeInfo> edgeInfos;

        DEFINE_STRUCT_VISITOR_FUNCTION(edgeInfos);
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

    // API: validateEdge
    struct ValidateEdgeRequest {
        NodeId sourceNode;
        std::string sourceSlot;
        NodeId targetNode;
        std::string targetSlot;

        DEFINE_STRUCT_VISITOR_FUNCTION(sourceNode, sourceSlot, targetNode, targetSlot);
    };
    struct ValidateEdgeResponse {
        grph::SlotState::Validity validity = grph::SlotState::Validity::VALID;
    
        DEFINE_STRUCT_VISITOR_FUNCTION(validity);
    };

    // API: getNodeStates
    struct GetNodeStatesRequest {
        std::vector<NodeId> nodeIds;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeIds);
    };
    struct GetNodeStatesResponse {
        std::vector<std::pair<NodeId, grph::NodeState>> nodeStates;
        DEFINE_STRUCT_VISITOR_FUNCTION(nodeStates);
    };

    // API: getSlotStates
   struct GetSlotStatesRequest {
        std::vector<SlotId> slotIds;
        std::vector<EncodedSlotId> slotIdsEncoded;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotIds, slotIdsEncoded);
    };
    struct GetSlotStatesResponse {
        std::vector<std::pair<SlotId, grph::SlotState>> slotStates;
        DEFINE_STRUCT_VISITOR_FUNCTION(slotStates);
    };

    // API: getAvailableFuncs
    struct GetAvailableFuncsRequest {
        // A cetegory filter is used to filter the returned functions.
        enum class CategoryFilter {
            UNKNOWN = 0,

            // Only functions whose URI is in the provided list will be returned.  
            URI_LIST = 1,

            // Only functions whose uri matches the provided prefix will be returned.
            PREFIX = 2,

            // Used for pagination.
            PAGE = 3,
        };

        // If no filter is provided, all functions will be returned.
        // If multiple filters are provided, they will be ANDed together
        // (i.e. only functions that satisfy all filters will be returned).
        std::vector<CategoryFilter> filters;

        // The following filter specific fields are used based on the value of `categoryFilter`.

        std::optional<std::vector<std::string>> uriList; // used when categoryFilter is URI_LIST
        std::optional<std::string> prefix; // used when categoryFilter is PREFIX
        std::optional<int32_t> pageStart;  // used when categoryFilter is PAGE
        std::optional<int32_t> pageSize;  // used when categoryFilter is PAGE

        DEFINE_ENUM_CONVERSION_FUNCTION(CategoryFilter, UNKNOWN, URI_LIST, PREFIX, PAGE);
        DEFINE_STRUCT_VISITOR_FUNCTION(filters, uriList, prefix, pageStart, pageSize);
    };
    struct GetAvailableFuncsResponse {
        std::vector<FunctionInfo> infos;
        DEFINE_STRUCT_VISITOR_FUNCTION(infos);
    };

    // API: setEncodedData
    struct SetEncodedDataRequest {
        // If set the data is for a input node, else it is for a slot.
        /// @deprecated: This field is no longer needed
        bool isNode = false;

        /// @deprecated: This field is no longer needed
        std::optional<NodeId> nodeId;

        std::optional<SlotId> slotId;

        // If this is not set, the existing manual data will be cleared.
        std::optional<grph::EncodedData> encodedData;

        DEFINE_STRUCT_VISITOR_FUNCTION(isNode, nodeId, slotId, encodedData);
    };

    // API: buildPipeline
    struct BuildPipelineResponse {
        std::vector<AssetInfo> assetInfos;
        DEFINE_STRUCT_VISITOR_FUNCTION(assetInfos);
    };

    // API: getResources
    struct GetResourcesResponse {
        std::vector<ResourceInfo> resources;

        DEFINE_STRUCT_VISITOR_FUNCTION(resources);
    };


    cppschema::ApiStub<VoidType, GetGraphMetaResponse> getGraphMeta;
    cppschema::ApiStub<SetGraphMetaRequest, VoidType> setGraphMeta;
    cppschema::ApiStub<VoidType, GetGraphResponse> getGraph;
    cppschema::ApiStub<CreateNodeRequest, CreateNodeResponse> createNode;
    cppschema::ApiStub<CreateIONodeRequest, CreateIONodeResponse> createIONode;
    cppschema::ApiStub<AddEdgesRequest, AddEdgesResponse> addEdges;
    cppschema::ApiStub<DeleteElementsRequest, DeleteElementsResponse> deleteElements;
    cppschema::ApiStub<ValidateEdgeRequest, ValidateEdgeResponse> validateEdge;
    cppschema::ApiStub<GetNodeStatesRequest, GetNodeStatesResponse> getNodeStates;
    cppschema::ApiStub<GetSlotStatesRequest, GetSlotStatesResponse> getSlotStates;
    cppschema::ApiStub<VoidType, VoidType> clearGraph;
    cppschema::ApiStub<GetAvailableFuncsRequest, GetAvailableFuncsResponse> getAvailableFuncs;
    cppschema::ApiStub<SetEncodedDataRequest, VoidType> setEncodedData;
    cppschema::ApiStub<VoidType, BuildPipelineResponse> buildPipeline;
    cppschema::ApiStub<VoidType, GetResourcesResponse> getResources;

    DEFINE_API_VISITOR_FUNCTION(
        getGraphMeta,
        setGraphMeta,
        getGraph,
        createNode,
        createIONode,
        addEdges,
        deleteElements,
        validateEdge,
        getNodeStates,
        getSlotStates,
        clearGraph,
        getAvailableFuncs,
        setEncodedData,
        buildPipeline,
        getResources);
};

}  // namespace ujcore
