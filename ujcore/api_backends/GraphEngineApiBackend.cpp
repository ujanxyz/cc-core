#include "ujcore/api_schemas/GraphEngineApi.hpp"
#include "ujcore/graph/GraphOps.hpp"

#include <iostream>
#include <memory>

#include "cppschema/apispec/api_registry.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/graph/GraphEngine.hpp"

namespace ujcore {

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    GraphEngineApiBackend() {
        InitializeGraphOps(graph_ops_);
    }

    std::string addNodeImpl(const std::string& payload) {
        return graph_ops_.insert_op->apply(payload, *engine_.mutable_state());
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
   GraphOps graph_ops_;
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
