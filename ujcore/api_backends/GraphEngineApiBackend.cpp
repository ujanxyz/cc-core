#include "ujcore/api_schemas/GraphEngineApi.hpp"

#include <iostream>
#include <memory>
#include <set>

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
    return j.dump();
}

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    GraphEngineApiBackend() {}

    std::string addElemsImpl(const std::string& payload_str) {
        json j = json::parse(payload_str);
        if (!j.contains("spec") || !j["spec"].is_object()) {
            j = {};
            j["status"] = "JSON_ERROR";
            return j.dump();
        }

        NodeFunctionSpec spec;
        if (!ParseNodeFuncSpecFromJsonObj(j["spec"], spec)) {
            j = {};
            j["status"] = "PARSE_ERROR";
            return j.dump();
        }
        
        EngineOpResult result;
        engine_.AddElements({spec}, result);
        return EngineResultTojson(result);
    }

    std::string deleteElemsImpl(const std::string& payload_str) {
        json j = json::parse(payload_str);

        std::set<std::string> node_ids;
        std::set<std::string> edge_ids;
        if (j.contains("nodes")) {
            j["nodes"].get_to(node_ids);
        }
        if (j.contains("edges")) {
            j["edges"].get_to(edge_ids);
        }

        EngineOpResult result;
        engine_.DeleteElements(node_ids, edge_ids, result);
        return EngineResultTojson(result);
    }

    GraphEngineApi::ElementStats getElemStatsImpl(const VoidType&) {
        const auto [num_nodes, num_edges, num_slots] = engine_.GetElementCounts();
        GraphEngineApi::ElementStats stats = {
            .num_nodes = num_nodes,
            .num_edges = num_edges,
            .num_slots = num_slots,
        };
        return stats;
    }

    VoidType clearGraphImpl(const VoidType&) {
        return VoidType{};
    }

 private:
   GraphEngine engine_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .addElems = &GraphEngineApiBackend::addElemsImpl,
        .deleteElems = &GraphEngineApiBackend::deleteElemsImpl,
        .getElemStats = &GraphEngineApiBackend::getElemStatsImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
