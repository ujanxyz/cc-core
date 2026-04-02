#include "ujcore/api_schemas/GraphEngineApi.hpp"

#include <set>

#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "nlohmann/json.hpp"
#include "ujcore/graph/GraphEngineImpl.h"

namespace ujcore {

using json = ::nlohmann::json;

static std::string EngineResultTojson(const EngineOpResult& result) {
    json j = {};
    if (!result.nodes_added.empty()) {
        auto arr = json::array();
        for (const auto& e : result.nodes_added) {
            arr.push_back(e);
        }
        j["nodesAdded"] = arr;
    }
    if (!result.edges_added.empty()) {
        auto arr = json::array();
        for (const auto& e : result.edges_added) {
            arr.push_back(e);
        }
        j["edgesAdded"] = arr;
    }
    if (!result.nodes_deleted.empty()) {
        auto arr = json::array();
        for (const auto& e : result.nodes_deleted) {
            arr.push_back(e);
        }
        j["nodesDeleted"] = arr;
    }
    if (!result.edges_deleted.empty()) {
        auto arr = json::array();
        for (const auto& e : result.edges_deleted) {
            arr.push_back(e);
        }
        j["edgesDeleted"] = arr;
    }
    if (result.topo_order.has_value()) {
        auto arr = json::array();
        for (const auto& e : result.topo_order.value()) {
            arr.push_back(e);
        }
        j["topoOrder"] = arr;
    }
    return j.dump();
}

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    using GraphDataResponse = GraphEngineApi::GraphDataResponse;
    using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
    using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
    using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;

    using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
    using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;

    using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;

    GraphEngineApiBackend() {}

    GraphDataResponse getGraphImpl(const VoidType& kvoid) {
        return GraphDataResponse {
            .nodeInfos = engine_.GetNodeInfos(),
            .edgeInfos = engine_.GetEdgeInfos(),
            .slotInfos = engine_.GetSlotInfos(),
        };
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        auto nodeInfo = engine_.AddNode(request.func);
        if (!nodeInfo.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfo.status();
        }
        return CreateNodeResponse {
            .nodeInfo = std::move(nodeInfo).value(),
        };
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
