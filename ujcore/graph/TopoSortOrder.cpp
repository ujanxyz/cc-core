#include "ujcore/graph/TopoSortOrder.h"

#include <algorithm>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace ujcore {

void TopoSortOrder::AddNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) {
        pos[u] = topo_order.size();
        topo_order.push_back(u);
        in_adj[u] = {};
        out_adj[u] = {};
        dirty_bit_ = true;
    }
}

void TopoSortOrder::RemoveNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) return;

    // Remove from adjacency lists of others
    for (auto& pair : in_adj) {
        pair.second.erase(u);
    }
    for (auto& pair : out_adj) {
        pair.second.erase(u);
    }
    in_adj.erase(u);
    out_adj.erase(u);

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
    if (in_adj.count(v)) in_adj[v].erase(u);
    if (out_adj.count(u)) out_adj[u].erase(v);
}

bool TopoSortOrder::AddEdge(const NodeId& u, const NodeId& v) {
    AddNode(u);
    AddNode(v);
    
    if (out_adj[u].count(v)) return true; // Already exists
    in_adj[v].insert(u);
    out_adj[u].insert(v);

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

bool TopoSortOrder::reorder(const NodeId& u, const NodeId& v) {
    std::vector<int> delta_f_indices;
    std::vector<int> delta_b_indices;

    if (!forward_dfs(v, pos[u], delta_f_indices)) {
        out_adj[u].erase(v);
        in_adj[v].erase(u);
        LOG(ERROR) << "Cycle detected! Edge " << u << "->" << v << " rejected";
        return false; // Cycle
    }    
    backward_dfs(u, pos[v], delta_b_indices);

    // Collect all affected indices in order
    std::vector<int> all_indices;
    for (int idx : delta_f_indices) all_indices.push_back(idx);
    for (int idx : delta_b_indices) all_indices.push_back(idx);
    std::sort(all_indices.begin(), all_indices.end());

    // Use the original topo_order to get nodes in their relative previous order
    std::vector<NodeId> reordered_nodes;    
    std::sort(delta_f_indices.begin(), delta_f_indices.end());
    std::sort(delta_b_indices.begin(), delta_b_indices.end());
    for (const int idx : delta_b_indices) reordered_nodes.push_back(topo_order[idx]);
    for (const int idx : delta_f_indices) reordered_nodes.push_back(topo_order[idx]);
    CHECK_EQ(reordered_nodes.size(), all_indices.size());

    // Update the actual topo_order and pos map.
    for (size_t i = 0; i < all_indices.size(); ++i) {
        int target_idx = all_indices[i];
        LOG(INFO) << "target_idx = " << target_idx;
        NodeId node = reordered_nodes[i];
        topo_order[target_idx] = node;
        pos[node] = target_idx;
    }

    dirty_bit_ = true;
    LOG(INFO) << "Order updated due to edge " << u << "->" << v;
    return true;
}

bool TopoSortOrder::forward_dfs(const NodeId& curr, int upper_bound, 
                         std::vector<int>& delta_f_indices) {
  std::vector<NodeId> stack;  // Emulates recursion stack for depth-first search
  std::unordered_set<NodeId> visited;
  stack.push_back(curr);
  while (!stack.empty()) {
    NodeId n = stack.back();
    stack.pop_back();

    if (visited.contains(n)) continue;
    visited.insert(n);
    delta_f_indices.push_back(pos[n]);
    for (const NodeId& nn : out_adj[n]) {
        if (pos[nn] == upper_bound) {
            return false;  // Cycle.
        }
        if (!visited.contains(nn) && pos[nn] < upper_bound) {
            stack.push_back(nn);
        }
    }
  }
  return true;
}

bool TopoSortOrder::backward_dfs(const NodeId& curr, int lower_bound, 
                         std::vector<int>& delta_b_indices) {
  std::vector<NodeId> stack;  // Emulates recursion stack for depth-first search
  std::unordered_set<NodeId> visited;
  stack.push_back(curr);
  while (!stack.empty()) {
    NodeId n = stack.back();
    stack.pop_back();

    if (visited.contains(n)) continue;
    visited.insert(n);
    delta_b_indices.push_back(pos[n]);
    for (const NodeId& nn : in_adj[n]) {
        if (!visited.contains(nn) && lower_bound < pos[nn]) {
            stack.push_back(nn);
        }
    }
  }
  return true;
}

}  // namespace ujcore