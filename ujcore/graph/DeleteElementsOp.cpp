#include "ujcore/graph/DeleteElementsOp.h"

#include "absl/log/log.h"

namespace ujcore {
namespace {

// Returns the set of actually deleted node ids.
std::set<std::string> DeleteNodesGetOrphanEdges(const std::set<std::string>& node_ids, GraphState& state, std::set<std::string>& orphan_edges) {
    std::set<std::string> deleted_node_ids;
    for (const std::string& node_id : node_ids) {
        const auto iter = state.nodes.find(node_id);
        if (iter == state.nodes.end()) continue;
        // TODO: Put the node's adjacent edges in to 'orphan_edges'.
        state.nodes.erase(iter);
        deleted_node_ids.insert(node_id);
    }
    return deleted_node_ids;
}

// Returns the set of actually deleted edge ids.
std::set<std::string> DeleteEdges(const std::set<std::string>& edge_ids, GraphState& state) {
    std::set<std::string> deleted_edge_ids;
    for (const std::string& edge_id : edge_ids) {
        const auto iter = state.edges.find(edge_id);
        if (iter == state.edges.end()) continue;
        // TODO: Queue any post-deletion action following the deletion of this edge.
        state.edges.erase(iter);
        deleted_edge_ids.insert(edge_id);
    }
    return deleted_edge_ids;
}

}  // namespace

void DoDeleteElemsOp(GraphOpsContext& ctx, DeleteElementsOpMutableIds& elem_ids) {
    std::set<std::string> unique_edge_ids(elem_ids.edge_ids.begin(), elem_ids.edge_ids.end());
    if (!elem_ids.node_ids.empty()) {
        elem_ids.node_ids = DeleteNodesGetOrphanEdges(elem_ids.node_ids, ctx.state, unique_edge_ids);
    }
    elem_ids.edge_ids = DeleteEdges(unique_edge_ids, ctx.state);
}

}  // namespace ujcore
