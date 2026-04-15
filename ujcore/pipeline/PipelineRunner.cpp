#include "ujcore/pipeline/PipelineRunner.h"

#include "absl/log/log.h"
#include "ujcore/function/FunctionRegistry.h"

namespace ujcore {

bool PipelineRunner::BuildFromState(const GraphState& state) {
    auto& registry = FunctionRegistry::GetInstance();
    nodes_.clear();
    for (const auto& [nodeId, node] : state.node_infos) {
        std::unique_ptr<FunctionSpec> fnSpec = registry.GetSpecFromUri(node.fnuri);
        std::unique_ptr<FunctionBase> fnInstance = registry.CreateFromUri(node.fnuri);
        if (fnSpec == nullptr || fnInstance == nullptr) {
            LOG(FATAL) << "Failed to create function instance for uri: " << node.fnuri;
        }
        std::unique_ptr<PipelineFnNode> fnNode = PipelineFnNode::Create(*fnSpec, std::move(fnInstance));
        nodes_[nodeId] = std::move(fnNode);
    }
    for (const auto& [edgeId, edge] : state.edge_infos) {

    }
    return true;
}

}  // namespace ujcore
