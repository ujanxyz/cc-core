#include "ujcore/api_schemas/GraphEngineApi.h"

#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/GraphState.h"
#include "ujcore/graph/GraphBuilder.h"
#include "ujcore/pipeline/PipelineRunner.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    using GetGraphResponse = GraphEngineApi::GetGraphResponse;
    using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
    using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
    using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;
    using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
    using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;
    using GetSlotStatesRequest = GraphEngineApi::GetSlotStatesRequest;
    using GetSlotStatesResponse = GraphEngineApi::GetSlotStatesResponse;
    using GetAvailableFuncsResponse = GraphEngineApi::GetAvailableFuncsResponse;

    using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;

    GraphEngineApiBackend(): topoSorter_(state_.topoSortState), builder_(state_, topoSorter_) {}

    GetGraphResponse getGraphImpl(const VoidType& kvoid) {
        return GetGraphResponse {
            .nodeInfos = builder_.GetNodeInfos(),
            .edgeInfos = builder_.GetEdgeInfos(),
            .slotInfos = builder_.GetSlotInfos(),
        };
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        auto nodeInfoOr = builder_.AddFuncNode(request.func);
        if (!nodeInfoOr.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfoOr.status();
        }
        plinfo::NodeInfo nodeInfo = std::move(nodeInfoOr).value();

        auto addNodeSlotInfos = [this, nodeRawId = nodeInfo.rawId](const std::vector<std::string>& slotNames, std::vector<plinfo::SlotInfo>& result) -> void {
            auto slotsOr = builder_.LookupNodeSlotInfos(NodeId(nodeRawId), slotNames);
            if (!slotsOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotsOr.status();
            }
            result = std::move(slotsOr).value();
        };
        auto addNodeSlotStates = [this, nodeRawId = nodeInfo.rawId](const std::vector<std::string>& slotNames, std::vector<plstate::SlotState>& result) -> void {
            std::vector<SlotId> slotIds;
            slotIds.reserve(slotNames.size());
            for (const std::string& slotName : slotNames) {
                slotIds.push_back(SlotId{nodeRawId, slotName});
            }
            auto slotStatesOr = builder_.LookupSlotStates(slotIds);
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
        return response;
    }

    AddEdgeResponse addEdgeImpl(const AddEdgeRequest& request) {
        auto edgeInfoOr = builder_.AddEdge(request.sourceNode, request.sourceSlot, request.targetNode, request.targetSlot);
        if (!edgeInfoOr.ok()) {
            LOG(FATAL) << "Add edge error: " << edgeInfoOr.status();
        }
        const plinfo::EdgeInfo edgeInfo = std::move(edgeInfoOr).value();

        auto state0Or = builder_.LookupSlotStates({ SlotId{edgeInfo.node0, edgeInfo.slot0} });
        auto state1Or = builder_.LookupSlotStates({ SlotId{edgeInfo.node1, edgeInfo.slot1} });
        if (!state0Or.ok() || state0Or->size() != 1) {
            LOG(FATAL) << "Lookup source slot state error: " << state0Or.status();
        }
        if (!state1Or.ok() || state1Or->size() != 1) {
            LOG(FATAL) << "Lookup target slot state error: " << state1Or.status();
        }

        plstate::SlotState& sourceState = std::move(state0Or).value()[0].second;
        plstate::SlotState& targetState = std::move(state1Or).value()[0].second;
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

    GetSlotStatesResponse getSlotStatesImpl(const GetSlotStatesRequest& request) {
        auto statesOr = builder_.LookupSlotStates(request.slotIds);
        return GetSlotStatesResponse {
            .slotStates = std::move(statesOr).value(),
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

    VoidType runPipelineImpl(const VoidType&) {
        auto buildResult = runner_.BuildFromState(state_);
        if (!buildResult.ok()) {
            LOG(FATAL) << "Build pipeline error: " << buildResult;
        }
        auto runResult = runner_.RunPipeline();
        if (!runResult.ok()) {
            LOG(FATAL) << "Run pipeline error: " << runResult;
        }
        return {};
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
        .addEdge = &GraphEngineApiBackend::addEdgeImpl,
        .deleteElements = &GraphEngineApiBackend::deleteElementsImpl,
        .getSlotStates = &GraphEngineApiBackend::getSlotStatesImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
        .getAvailableFuncs = &GraphEngineApiBackend::getAvailableFuncsImpl,
        .runPipeline = &GraphEngineApiBackend::runPipelineImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
