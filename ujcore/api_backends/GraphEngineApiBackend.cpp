#include "ujcore/api_schemas/GraphEngineApi.h"

#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/graph/GraphBuilder.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/PipelineRunner.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    using GetGraphResponse = GraphEngineApi::GetGraphResponse;
    using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
    using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;
    using CreateIONodeRequest = GraphEngineApi::CreateIONodeRequest;
    using CreateIONodeResponse = GraphEngineApi::CreateIONodeResponse;
    using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
    using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;
    using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
    using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;
    using ValidateEdgeRequest = GraphEngineApi::ValidateEdgeRequest;
    using ValidateEdgeResponse = GraphEngineApi::ValidateEdgeResponse;
    using GetNodeStatesRequest = GraphEngineApi::GetNodeStatesRequest;
    using GetNodeStatesResponse = GraphEngineApi::GetNodeStatesResponse;
    using GetSlotStatesRequest = GraphEngineApi::GetSlotStatesRequest;
    using GetSlotStatesResponse = GraphEngineApi::GetSlotStatesResponse;
    using GetAvailableFuncsResponse = GraphEngineApi::GetAvailableFuncsResponse;
    using SetEncodedDataRequest = GraphEngineApi::SetEncodedDataRequest;
    using BuildPipelineResponse = GraphEngineApi::BuildPipelineResponse;
    using RunPipelineResponse = GraphEngineApi::RunPipelineResponse;
    using GetResourcesResponse = GraphEngineApi::GetResourcesResponse;

    GraphEngineApiBackend(): topoSorter_(state_.topoSortState), builder_(state_, topoSorter_) {}

    GetGraphResponse getGraphImpl(const VoidType& kvoid) {
        return GetGraphResponse {
            .nodeInfos = GraphUtils::GetAllNodeInfos(state_),
            .edgeInfos = GraphUtils::GetAllEdgeInfos(state_),
            .slotInfos = GraphUtils::GetAllSlotInfos(state_),
        };
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        auto nodeInfoOr = builder_.AddFuncNode(request.func);
        if (!nodeInfoOr.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfoOr.status();
        }
        const NodeId nodeId = nodeInfoOr->rawId;
        const plinfo::NodeInfo nodeInfo = std::move(nodeInfoOr).value();

        auto addNodeSlotInfos = [this, nodeRawId = nodeId](const std::vector<std::string>& slotNames, std::vector<plinfo::SlotInfo>& result) -> void {
            auto slotsOr = builder_.LookupNodeSlotInfos(NodeId(nodeRawId), slotNames);
            if (!slotsOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotsOr.status();
            }
            result = std::move(slotsOr).value();
        };
        auto addNodeSlotStates = [this, nodeRawId = nodeId](const std::vector<std::string>& slotNames, std::vector<plstate::SlotState>& result) -> void {
            std::vector<SlotId> slotIds;
            slotIds.reserve(slotNames.size());
            for (const std::string& slotName : slotNames) {
                slotIds.push_back(SlotId{nodeRawId, slotName});
            }
            auto slotStatesOr = GraphUtils::LookupSlotStates(state_, slotIds);
            if (!slotStatesOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotStatesOr.status();
            }
            std::vector<plstate::SlotState> states;
            states.reserve(slotStatesOr->size());
            for (auto& [_, slotState] : std::move(slotStatesOr).value()) {
                states.push_back(std::move(slotState));
            }
            result = std::move(states);
        };

        CreateNodeResponse response;
        addNodeSlotInfos(nodeInfo.ins, response.inInfos);
        addNodeSlotInfos(nodeInfo.outs, response.outInfos);
        addNodeSlotInfos(nodeInfo.inouts, response.inoutInfos);
        addNodeSlotStates(nodeInfo.ins, response.inStates);
        addNodeSlotStates(nodeInfo.outs, response.outStates);
        addNodeSlotStates(nodeInfo.inouts, response.inoutStates);
        response.nodeInfo = std::move(nodeInfo);
        response.nodeState = GraphUtils::CopyNodeState(state_, nodeId);
        return response;
    }

    CreateIONodeResponse createIONodeImpl(const CreateIONodeRequest& request) {
        auto nodeSlotInfoOr = builder_.AddIONode(request.dtype, request.isOutput);
        if (!nodeSlotInfoOr.ok()) {
            LOG(FATAL) << "Insert IO node error: " << nodeSlotInfoOr.status();
        }
        plinfo::NodeInfo nodeInfo;
        plinfo::SlotInfo slotInfo;
        std::tie(nodeInfo, slotInfo) = std::move(nodeSlotInfoOr).value();

        const NodeId nodeId = nodeInfo.rawId;
        auto slotStateOr = GraphUtils::LookupSlotStates(state_, { SlotId{nodeId, slotInfo.name} });
        if (!slotStateOr.ok() || slotStateOr->size() != 1) {
            LOG(FATAL) << "Lookup IO node slot state error: " << slotStateOr.status();
        }
        const plstate::SlotState slotState = std::move(slotStateOr).value().begin()->second;
        return CreateIONodeResponse {
            .nodeInfo = std::move(nodeInfo),
            .nodeState = GraphUtils::CopyNodeState(state_, nodeId),
            .slotInfo = std::move(slotInfo),
            .slotState = std::move(slotState),
        };
    }

    AddEdgeResponse addEdgeImpl(const AddEdgeRequest& request) {
        auto edgeInfoOr = builder_.AddEdge(request.sourceNode, request.sourceSlot, request.targetNode, request.targetSlot);
        if (!edgeInfoOr.ok()) {
            LOG(FATAL) << "Add edge error: " << edgeInfoOr.status();
        }
        const plinfo::EdgeInfo edgeInfo = std::move(edgeInfoOr).value();

        auto state0Or = GraphUtils::LookupSlotStates(state_, { SlotId{edgeInfo.node0, edgeInfo.slot0} });
        auto state1Or = GraphUtils::LookupSlotStates(state_, { SlotId{edgeInfo.node1, edgeInfo.slot1} });
        if (!state0Or.ok() || state0Or->size() != 1) {
            LOG(FATAL) << "Lookup source slot state error: " << state0Or.status();
        }
        if (!state1Or.ok() || state1Or->size() != 1) {
            LOG(FATAL) << "Lookup target slot state error: " << state1Or.status();
        }

        plstate::SlotState& sourceState = std::move(state0Or).value().begin()->second;
        plstate::SlotState& targetState = std::move(state1Or).value().begin()->second;
        sourceState.outEdges.insert(edgeInfo.id);
        targetState.inEdges.insert(edgeInfo.id);

        return AddEdgeResponse {
            .edgeInfo = std::move(edgeInfo),
            .sourceState = sourceState,
            .targetState = targetState,
        };
    }

    DeleteElementsResponse deleteElementsImpl(const DeleteElementsRequest& request) {
        auto deleteResultOr = builder_.DeleteElements(request.nodeIds, request.edgeIds);
        if (!deleteResultOr.ok()) {
            LOG(FATAL) << "Delete elements error: " << deleteResultOr.status();
        }

        std::vector<EdgeId> deletedEdgeIds;
        std::set<SlotId> deletedSlotIds;
        std::set<SlotId> affectedSlotIds;
        std::tie(deletedEdgeIds, deletedSlotIds, affectedSlotIds) = std::move(deleteResultOr).value();

        return DeleteElementsResponse {
            .nodeIds = request.nodeIds,
            .edgeIds = std::move(deletedEdgeIds),
            .deletedSlotIds = std::vector<SlotId>(deletedSlotIds.begin(), deletedSlotIds.end()),
            .affectedSlotIds = std::vector<SlotId>(affectedSlotIds.begin(), affectedSlotIds.end()),
        };
    }

    ValidateEdgeResponse validateEdgeImpl(const ValidateEdgeRequest& request) {
        SlotId sourceSlotId{request.sourceNode, request.sourceSlot};
        SlotId targetSlotId{request.targetNode, request.targetSlot};
        auto validateResultOr = builder_.ValidateEdge(sourceSlotId, targetSlotId);
        if (!validateResultOr.ok()) {
            LOG(FATAL) << "Validate edge error: " << validateResultOr.status();
        }
        plstate::SlotState::Validity validity = std::move(validateResultOr).value();
        return ValidateEdgeResponse {
            .validity = validity,
        };
    }

    GetNodeStatesResponse getNodeStatesImpl(const GetNodeStatesRequest& request) {
        auto nodeStates = GraphUtils::LookupNodeStates(state_, request.nodeIds);
        if (!nodeStates.ok()) {
            LOG(FATAL) << "Lookup node states error: " << nodeStates.status();
        }
        std::vector<std::pair<NodeId, plstate::NodeState>> result;
        for (auto& [nodeId, nodeState] : std::move(nodeStates).value()) {
            result.push_back({nodeId, std::move(nodeState)});
        }
        return GetNodeStatesResponse {
            .nodeStates = std::move(result),
        };
    }

    GetSlotStatesResponse getSlotStatesImpl(const GetSlotStatesRequest& request) {
        auto slotStates = GraphUtils::LookupSlotStates(state_, request.slotIds);
        if (!slotStates.ok()) {
            LOG(FATAL) << "Lookup slot states error: " << slotStates.status();
        }
        std::vector<std::pair<SlotId, plstate::SlotState>> result;
        for (auto& [slotId, slotState] : std::move(slotStates).value()) {
            result.push_back({slotId, std::move(slotState)});
        }
        return GetSlotStatesResponse {
            .slotStates = std::move(result),
        };
    }

    VoidType clearGraphImpl(const VoidType&) {
        auto countOr = builder_.ClearGraph();
        if (!countOr.ok()) {
            LOG(FATAL) << "Clear graph error: " << countOr.status();
        }
        return {};
    }

    GetAvailableFuncsResponse getAvailableFuncsImpl(const VoidType&) {
        auto allInfos = builder_.GetAvailableFuncInfos();
        if (!allInfos.ok()) {
            LOG(FATAL) << "Get all func infos error: " << allInfos.status();
        }
        return GetAvailableFuncsResponse {
            .infos = std::move(allInfos).value(),
        };
    }

    VoidType setEncodedDataImpl(const SetEncodedDataRequest& request) {
        if (request.isNode) {
            if (request.nodeId.has_value() == false) {
                LOG(FATAL) << "Node id must be set for node encoded data.";
            }
            const NodeId nodeId = request.nodeId.value();
            auto status = builder_.SetNodeEncodedData(nodeId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set node encoded data error: " << status;
            }
        } else {
            if (request.slotId.has_value() == false) {
                LOG(FATAL) << "Slot id must be set for slot encoded data.";
            }
            const SlotId slotId = request.slotId.value();
            auto status = builder_.SetSlotEncodedData(slotId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set slot encoded data error: " << status;
            }
        }
        return {};
    }

    BuildPipelineResponse buildPipelineImpl(const VoidType&) {
        auto assetInfosOr = runner_.RebuildFromState(state_);
        if (!assetInfosOr.ok()) {
            LOG(FATAL) << "Build pipeline error: " << assetInfosOr.status();
        }
        return BuildPipelineResponse {
            .assetInfos = std::move(assetInfosOr).value(),
        };
    }

    RunPipelineResponse runPipelineImpl(const VoidType& request) {
        RunPipelineResponse response;
        auto runResult = runner_.RunPipeline();
        if (!runResult.ok()) {
            LOG(FATAL) << "Run pipeline error: " << runResult.status();
        }
        response.outputs = std::move(runResult).value();
        return response;
    }

    GetResourcesResponse getResourcesImpl(const VoidType&) {
        auto resourcesOr = runner_.GetPipelineResources();
        if (!resourcesOr.ok()) {
            LOG(FATAL) << "Get pipeline resources error: " << resourcesOr.status();
        }
        return GetResourcesResponse {
            .resources = std::move(resourcesOr).value(),
        };
    }

 private:
   GraphState state_;
   TopoSortOrder topoSorter_;
   GraphBuilder builder_;
   PipelineRunner runner_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .getGraph = &GraphEngineApiBackend::getGraphImpl,
        .createNode = &GraphEngineApiBackend::createNodeImpl,
        .createIONode = &GraphEngineApiBackend::createIONodeImpl,
        .addEdge = &GraphEngineApiBackend::addEdgeImpl,
        .deleteElements = &GraphEngineApiBackend::deleteElementsImpl,
        .validateEdge = &GraphEngineApiBackend::validateEdgeImpl,
        .getNodeStates = &GraphEngineApiBackend::getNodeStatesImpl,
        .getSlotStates = &GraphEngineApiBackend::getSlotStatesImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
        .getAvailableFuncs = &GraphEngineApiBackend::getAvailableFuncsImpl,
        .setEncodedData = &GraphEngineApiBackend::setEncodedDataImpl,
        .buildPipeline = &GraphEngineApiBackend::buildPipelineImpl,
        .runPipeline = &GraphEngineApiBackend::runPipelineImpl,
        .getResources = &GraphEngineApiBackend::getResourcesImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
