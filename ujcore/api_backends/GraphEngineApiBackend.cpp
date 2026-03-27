#include "ujcore/api_schemas/GraphEngineApi.hpp"

#include <iostream>
#include <memory>
#include <set>

#include "absl/log/log.h"
#include "cppschema/apispec/api_registry.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "nlohmann/json.hpp"
#include "ujcore/functions/FunctionIoUtils.hpp"
#include "ujcore/graph/GraphEngine.hpp"

namespace ujcore {

using json = ::nlohmann::json;

std::string EngineResultTojson(const EngineOpResult& result) {
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

static NodeFunctionSpec ParseNodeFunction(const std::string& payload_str) {
    json j = json::parse(payload_str);
    if (!(j.contains("spec") && j["spec"].is_object())) {
        LOG(FATAL) << "Object 'spec' not found in json: " << payload_str;
    }
    NodeFunctionSpec spec;
    if (!ParseNodeFuncSpecFromJsonObj(j["spec"], spec)) {
        LOG(FATAL) << "Failed t parse NodeFunctionSpec from json";
    }
    return spec;
}


class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    using AddEdgesRequest = GraphEngineApi::AddEdgesRequest;
    using AddEdgesResponse = GraphEngineApi::AddEdgesResponse;

    using GraphDataResponse = GraphEngineApi::GraphDataResponse;
    using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;

    GraphEngineApiBackend() {}

    GraphDataResponse getGraphImpl(const VoidType& kvoid) {
        return GraphDataResponse {
            .nodes = engine_.GetNodes(),
            .edges = engine_.GetEdges(),
            .slots = engine_.GetSlots(),
        };
    }

    CreateNodeResponse createNodeImpl(const std::string& spec_json) {
        NodeFunctionSpec fnspec = ParseNodeFunction(spec_json);
        absl::StatusOr<data::GraphNode> node_or = engine_.InsertNode(fnspec);
        if (!node_or.ok()) {
            LOG(FATAL) << "Insert node error: " << node_or.status();
        }
        return CreateNodeResponse {
            .node = std::move(node_or).value(),
            .edges = {},
        };
    }

    AddEdgesResponse addEdgesImpl(const AddEdgesRequest& request) {
        auto edges_or = engine_.AddEdges(request.entries);
        if (!edges_or.ok()) {
            LOG(FATAL) << "Add edges error: " << edges_or.status();
        }
        return AddEdgesResponse {
            .edges = std::move(edges_or).value(),
        };
    }

    data::NodeAndEdgeIds deleteElementsImpl(const data::NodeAndEdgeIds& input) {
        auto deleted_ids_or = engine_.DeleteElements(input);
        if (!deleted_ids_or.ok()) {
            LOG(FATAL) << "Delete elements error: " << deleted_ids_or.status();
        }
        return std::move(deleted_ids_or).value();
    }

    data::NodeAndEdgeIds clearGraphImpl(const VoidType&) {
        return {};
    }

 private:
   GraphEngine engine_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .getGraph = &GraphEngineApiBackend::getGraphImpl,
        .createNode = &GraphEngineApiBackend::createNodeImpl,
        .addEdges = &GraphEngineApiBackend::addEdgesImpl,
        .deleteElements = &GraphEngineApiBackend::deleteElementsImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
