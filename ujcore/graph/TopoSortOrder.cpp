#include "ujcore/graph/TopoSortOrder.h"

#include "absl/log/log.h"

namespace ujcore {

void TopoSortOrder::AddNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) {
        pos[u] = topo_order.size();
        topo_order.push_back(u);
        adj[u] = {};
        dirty_bit_ = true;
    }
}

void TopoSortOrder::RemoveNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) return;

    // Remove from adjacency lists of others
    for (auto& pair : adj) {
        pair.second.erase(u);
    }
    adj.erase(u);

    // Remove from topo_order and update positions
    int idx = pos[u];
    topo_order.erase(topo_order.begin() + idx);
    pos.erase(u);
    for (int i = idx; i < topo_order.size(); ++i) {
        pos[topo_order[i]] = i;
    }
    dirty_bit_ = true;
}

void TopoSortOrder::RemoveEdge(const NodeId& u, const NodeId& v) {
    if (adj.count(u)) adj[u].erase(v);
}

bool TopoSortOrder::AddEdge(const NodeId& u, const NodeId& v) {
    AddNode(u);
    AddNode(v);
    
    if (adj[u].count(v)) return true; // Already exists
    adj[u].insert(v);

    if (pos[u] < pos[v]) return true; // Order still valid

    // Order violated: Reorder affected region
    return reorder(u, v);
}

void TopoSortOrder::PrintTopoOrder() {
    LOG(INFO) << "Topo Sort Order:";
    for (const auto& n : topo_order) {
        LOG(INFO) << n;
    }
}

bool TopoSortOrder::reorder(NodeId u, NodeId v) {
    std::vector<NodeId> delta_f, delta_b;
    int lower = pos[v];
    int upper = pos[u];

    // 1. Find affected nodes forward from v and backward from u
    if (!visited_forward(v, upper, delta_f)) {
        LOG(ERROR) << "Cycle detected! Edge " << u << "->" << v << " rejected";
        adj[u].erase(v);
        return false;
    }
    visited_backward(u, lower, delta_b);

    // 2. Sort affected nodes by their current topological positions
    auto sort_by_pos = [&](const NodeId& a, const NodeId& b) { return pos[a] < pos[b]; };
    std::sort(delta_b.begin(), delta_b.end(), sort_by_pos);
    std::sort(delta_f.begin(), delta_f.end(), sort_by_pos);

    // 3. Shift nodes: Backward nodes move after Forward nodes
    std::vector<NodeId> merged;
    merged.insert(merged.end(), delta_f.begin(), delta_f.end());
    merged.insert(merged.end(), delta_b.begin(), delta_b.end());

    std::vector<int> affected_indices;
    for (const auto& node : delta_b) affected_indices.push_back(pos[node]);
    for (const auto& node : delta_f) affected_indices.push_back(pos[node]);
    std::sort(affected_indices.begin(), affected_indices.end());

    for (int i = 0; i < merged.size(); ++i) {
        topo_order[affected_indices[i]] = merged[i];
        pos[merged[i]] = affected_indices[i];
    }
    dirty_bit_ = true;

    LOG(INFO) << "Order updated due to edge " << u << "->" << v;
    return true;
}

bool TopoSortOrder::visited_forward(const NodeId& curr, int upper_bound, std::vector<NodeId>& delta_f) {
    delta_f.push_back(curr);
    for (const auto& next : adj[curr]) {
        if (pos[next] == upper_bound) return false; // Cycle found
        if (pos[next] < upper_bound) {
            if (!visited_forward(next, upper_bound, delta_f)) return false;
        }
    }
    return true;
}

void TopoSortOrder::visited_backward(const NodeId& curr, int lower_bound, std::vector<NodeId>& delta_b) {
    delta_b.push_back(curr);
    // This requires an inverse adjacency list for efficiency, 
    // simplified here for demonstration using a search.
    for (auto const& [prev, neighbors] : adj) {
        if (neighbors.count(curr) && pos[prev] > lower_bound) {
            visited_backward(prev, lower_bound, delta_b);
        }
    }
}

}  // namespace ujcore