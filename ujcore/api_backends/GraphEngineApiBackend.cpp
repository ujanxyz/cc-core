#include "ujcore/api_schemas/GraphEngineApi.hpp"

#include <memory>

#include "cppschema/apispec/api_registry.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "nlohmann/json.hpp"
#include "ujcore/graph/GraphEngine.hpp"

namespace ujcore {

using ::nlohmann::json;

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    GraphEngineApiBackend() {
        engine_ = std::make_unique<GraphEngine>();
    }

    std::string addNodeImpl(const std::string&) {
        json ex1 = json::parse(R"(
            {
                "pi": 3.141,
                "happy": true
            })");
        std::string func_spec_jsonstr = ex1.dump();
        
        const std::string node_id = engine_->InsertNewNode(func_spec_jsonstr);
        return node_id;
    }

    GraphEngineApi::ElementStats getElemStatsImpl(const VoidType&) {
        const auto [num_nodes, num_edges, num_slots] = engine_->GetElementCounts();
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
   std::unique_ptr<GraphEngine> engine_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .addNode = &GraphEngineApiBackend::addNodeImpl,
        .getElemStats = &GraphEngineApiBackend::getElemStatsImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
