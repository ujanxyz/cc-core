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
    if (result.topo_order.has_value()) {
        auto arr = json::array();
        for (const auto& e : result.topo_order.value()) {
            arr.push_back(e);
        }
        j["topoOrder"] = arr;
    }
    return j.dump();
}

static bool ParseString(const json& j, const char* key, std::string& out) {
    if (!j.contains(key) || !j[key].is_string()) return false;
    out = j[key].get<std::string>();
    return true;
}


class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    GraphEngineApiBackend() {}

    std::string addElemsImpl(const std::string& payload_str) {
        json j = json::parse(payload_str);
        EngineOpResult result;
        if (j.contains("spec") && j["spec"].is_object()) {
            // Insert nodes.
            NodeFunctionSpec spec;
            if (!ParseNodeFuncSpecFromJsonObj(j["spec"], spec)) {
                j = {};
                j["status"] = "PARSE_ERROR";
                return j.dump();
            }
            engine_.AddElements({spec}, result);
        } else if (j.contains("edges") && j["edges"].is_array()) {
            std::vector<EdgeData> edges;
            for (const auto& je : j["edges"]) {
                if (!je.is_object()) {
                    j = {};
                    j["status"] = "PARSE_ERROR";
                    return j.dump();
                }
                EdgeData edge;
                const bool parse_ok =
                    ParseString(je, "srcNode", edge.srcNode) &&
                    ParseString(je, "destNode", edge.destNode) &&
                    ParseString(je, "srcSlot", edge.srcSlot) &&
                    ParseString(je, "destSlot", edge.destSlot);
                if (!parse_ok) {
                    j = {};
                    j["status"] = "PARSE_ERROR: Edge";
                    return j.dump();
                }
                edges.push_back(std::move(edge));
            }
            engine_.AddEdgeConnections(edges, result);
        } else {
            j = {};
            j["status"] = "JSON_ERROR";
            return j.dump();
        }

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
