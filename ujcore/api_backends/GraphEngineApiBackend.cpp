#include "ujcore/api_schemas/GraphEngineApi.h"

#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/graph/GraphEngineImpl.h"

namespace ujcore {

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    using GetGraphResponse = GraphEngineApi::GetGraphResponse;
    using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
    using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
    using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;

    using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
    using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;

    using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;

    GraphEngineApiBackend() {}

    GetGraphResponse getGraphImpl(const VoidType& kvoid) {
        return GetGraphResponse {
            .nodeInfos = engine_.GetNodeInfos(),
            .edgeInfos = engine_.GetEdgeInfos(),
            .slotInfos = engine_.GetSlotInfos(),
        };
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        auto nodeInfoOr = engine_.AddNode(request.func);
        if (!nodeInfoOr.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfoOr.status();
        }
        plinfo::NodeInfo nodeInfo = std::move(nodeInfoOr).value();

        auto lookupNodeSlots = [this, nodeId = nodeInfo.id](const std::vector<std::string>& slotNames, std::vector<plinfo::SlotInfo>& result) -> void {
            auto slotsOr = engine_.LookupNodeSlots(nodeId, slotNames);
            if (!slotsOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotsOr.status();
            }
            result = std::move(slotsOr).value();
        };

        CreateNodeResponse response;
        lookupNodeSlots(nodeInfo.ins, response.ins);
        lookupNodeSlots(nodeInfo.outs, response.outs);
        lookupNodeSlots(nodeInfo.inouts, response.inouts);
        response.nodeInfo = std::move(nodeInfo);
        return response;
    }

    AddEdgeResponse addEdgeImpl(const AddEdgeRequest& request) {
        auto edgeInfo = engine_.AddEdge(request.sourceNode, request.sourceSlot, request.targetNode, request.targetSlot);
        if (!edgeInfo.ok()) {
            LOG(FATAL) << "Add edge error: " << edgeInfo.status();
        }
        return AddEdgeResponse {
            .edgeInfo = std::move(edgeInfo).value(),
        };
    }

    DeleteElementsResponse deleteElementsImpl(const DeleteElementsRequest& request) {
        auto deletedIdsOr = engine_.DeleteElements(request.nodeIds, request.edgeIds);
        if (!deletedIdsOr.ok()) {
            LOG(FATAL) << "Delete elements error: " << deletedIdsOr.status();
        }
        const std::vector<std::string> deletedEdgeIds = std::move(deletedIdsOr).value();
        return DeleteElementsResponse {
            .nodeIds = request.nodeIds,
            .edgeIds = std::move(deletedEdgeIds),
        };
    }

    VoidType clearGraphImpl(const VoidType&) {
        auto countOr = engine_.ClearGraph();
        if (!countOr.ok()) {
            LOG(FATAL) << "Clear graph error: " << countOr.status();
        }
        return {};
    }

 private:
   GraphEngineImpl engine_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .getGraph = &GraphEngineApiBackend::getGraphImpl,
        .createNode = &GraphEngineApiBackend::createNodeImpl,
        .addEdge = &GraphEngineApiBackend::addEdgeImpl,
        .deleteElements = &GraphEngineApiBackend::deleteElementsImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
