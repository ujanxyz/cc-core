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

// Returns the collection of actually deleted edges.
std::map<std::string, EdgeData> DeleteEdges(const std::set<std::string>& edge_ids, GraphState& state) {
    std::map<std::string, EdgeData> deleted_edges;
    for (const std::string& edge_id : edge_ids) {
        const auto iter = state.edges.find(edge_id);
        if (iter == state.edges.end()) continue;
        // TODO: Queue any post-deletion action following the deletion of this edge.
        deleted_edges[edge_id] = std::move(iter->second);
        state.edges.erase(iter);
    }
    return deleted_edges;
}

}  // namespace

void DoDeleteElemsOp(GraphOpsContext& ctx, DeleteElementsOpMutableIds& elem_ids) {
    auto* const toposort_order = ctx.toposort_order;
    std::set<std::string> unique_edge_ids(elem_ids.edge_ids.begin(), elem_ids.edge_ids.end());
    if (!elem_ids.node_ids.empty()) {
        elem_ids.node_ids = DeleteNodesGetOrphanEdges(elem_ids.node_ids, ctx.state, unique_edge_ids);
    }
    auto deleted_edges = DeleteEdges(unique_edge_ids, ctx.state);
    for (const auto& node_id : elem_ids.node_ids) {
        toposort_order->RemoveNode(node_id);
    }
    elem_ids.edge_ids.clear();
    for (const auto& [edge_id, edge] : deleted_edges) {
        elem_ids.edge_ids.insert(edge_id);
        toposort_order->RemoveEdge(edge.srcNode, edge.destNode);
    }
}

}  // namespace ujcore
