#include "ujcore/data/TopoSortOrder.h"

#include <algorithm>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace ujcore {

TopoSortOrder::TopoSortOrder(TopoSortState& state) : state_(state) {}

void TopoSortOrder::AddNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) {
        pos[u] = state_.sortOrder.size();
        state_.sortOrder.push_back(u);
        inAdj[u] = {};
        outAdj[u] = {};
        state_.hasDirtyBit = true;
    }
}

void TopoSortOrder::RemoveNode(const NodeId& u) {
    if (pos.find(u) == pos.end()) return;

    // Remove from adjacency lists of others
    for (auto& pair : inAdj) {
        pair.second.erase(u);
    }
    for (auto& pair : outAdj) {
        pair.second.erase(u);
    }
    inAdj.erase(u);
    outAdj.erase(u);

    // Remove from state_.sortOrder and update positions
    int idx = pos[u];
    state_.sortOrder.erase(state_.sortOrder.begin() + idx);
    pos.erase(u);
    for (int i = idx; i < state_.sortOrder.size(); ++i) {
        pos[state_.sortOrder[i]] = i;
    }
    state_.hasDirtyBit = true;
}

void TopoSortOrder::RemoveEdge(const NodeId& u, const NodeId& v) {
    if (inAdj.count(v)) inAdj[v].erase(u);
    if (outAdj.count(u)) outAdj[u].erase(v);
}

bool TopoSortOrder::AddEdge(const NodeId& u, const NodeId& v) {
    AddNode(u);
    AddNode(v);
    
    if (outAdj[u].count(v)) return true; // Already exists
    inAdj[v].insert(u);
    outAdj[u].insert(v);

    if (pos[u] < pos[v]) return true; // Order still valid
    // Order violated: Reorder affected region
    return reorder(u, v);
}

void TopoSortOrder::PrintTopoOrder() {
    LOG(INFO) << "Topo Sort Order:";
    for (const NodeId n : state_.sortOrder) {
        LOG(INFO) << n.value;
    }
}

bool TopoSortOrder::reorder(const NodeId& u, const NodeId& v) {
    std::vector<int> delta_f_indices;
    std::vector<int> delta_b_indices;

    if (!forward_dfs(v, pos[u], delta_f_indices)) {
        outAdj[u].erase(v);
        inAdj[v].erase(u);
        LOG(ERROR) << "Cycle detected! Edge " << u.value << "->" << v.value << " rejected";
        return false; // Cycle
    }    
    backward_dfs(u, pos[v], delta_b_indices);

    // Collect all affected indices in order
    std::vector<int> all_indices;
    for (int idx : delta_f_indices) all_indices.push_back(idx);
    for (int idx : delta_b_indices) all_indices.push_back(idx);
    std::sort(all_indices.begin(), all_indices.end());

    // Use the original state_.sortOrder to get nodes in their relative previous order
    std::vector<NodeId> reordered_nodes;    
    std::sort(delta_f_indices.begin(), delta_f_indices.end());
    std::sort(delta_b_indices.begin(), delta_b_indices.end());
    for (const int idx : delta_b_indices) reordered_nodes.push_back(state_.sortOrder[idx]);
    for (const int idx : delta_f_indices) reordered_nodes.push_back(state_.sortOrder[idx]);
    CHECK_EQ(reordered_nodes.size(), all_indices.size());

    // Update the actual state_.sortOrder and pos map.
    for (size_t i = 0; i < all_indices.size(); ++i) {
        int target_idx = all_indices[i];
        NodeId node = reordered_nodes[i];
        state_.sortOrder[target_idx] = node;
        pos[node] = target_idx;
    }

    state_.hasDirtyBit = true;
    LOG(INFO) << "Order updated due to edge " << u.value << "->" << v.value;
    return true;
}

bool TopoSortOrder::forward_dfs(const NodeId& curr, int upper_bound, 
                         std::vector<int>& delta_f_indices) {
  std::vector<NodeId> stack;  // Emulates recursion stack for depth-first search
  std::set<NodeId> visited;
  stack.push_back(curr);
  while (!stack.empty()) {
    NodeId n = stack.back();
    stack.pop_back();

    if (visited.contains(n)) continue;
    visited.insert(n);
    delta_f_indices.push_back(pos[n]);
    for (const NodeId nn : outAdj[n]) {
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
  std::set<NodeId> visited;
  stack.push_back(curr);
  while (!stack.empty()) {
    NodeId n = stack.back();
    stack.pop_back();

    if (visited.contains(n)) continue;
    visited.insert(n);
    delta_b_indices.push_back(pos[n]);
    for (const NodeId nn : inAdj[n]) {
        if (!visited.contains(nn) && lower_bound < pos[nn]) {
            stack.push_back(nn);
        }
    }
  }
  return true;
}

}  // namespace ujcore