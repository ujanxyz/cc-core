#pragma once

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace ujcore {

class TopoSortOrder {
 public:
    using NodeId = std::string;

    void AddNode(const NodeId& u);
    void RemoveNode(const NodeId& u);
    void RemoveEdge(const NodeId& u, const NodeId& v);
    bool AddEdge(const NodeId& u, const NodeId& v);

    void PrintTopoOrder();

    const std::vector<NodeId>& CurrentOrder() const {
        return topo_order;
    }

    bool HasDirtyBitSet() const {
        return dirty_bit_;
    }

    void ClearDirtyBit() {
        dirty_bit_ = false;
    }

 private:
    bool reorder(const NodeId& u, const NodeId& v);
    bool forward_dfs(const NodeId& curr, int upper_bound,
                         std::vector<int>& delta_f_indices);
    bool backward_dfs(const NodeId& curr, int lower_bound, 
                         std::vector<int>& delta_b_indices);

    // Is the order modified since last clear dirty bit call.
    bool dirty_bit_ {false};

    std::vector<NodeId> topo_order;
    std::map<NodeId, int> pos; // node -> index in topo_order
    std::map<NodeId, std::set<NodeId>> in_adj;
    std::map<NodeId, std::set<NodeId>> out_adj;

    friend class TopoSortOrderTest;
};

}  // namespace ujcore