#pragma once

#include <map>
#include <set>
#include <string>
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

    bool HasDirtyBitSet() const {
        return dirty_bit_;
    }

    void ClearDirtyBit() {
        dirty_bit_ = false;
    }

    const std::vector<NodeId>& CurrentOrder() const {
        return topo_order;
    }

 private:
    bool reorder(NodeId u, NodeId v);
    bool visited_forward(const NodeId& curr, int upper_bound, std::vector<NodeId>& delta_f);
    void visited_backward(const NodeId& curr, int lower_bound, std::vector<NodeId>& delta_b);

    std::vector<NodeId> topo_order;
    // Is the order modified since last clear dirty bit call.
    bool dirty_bit_ {false};

    std::map<NodeId, std::set<NodeId>> adj;
    std::map<NodeId, int> pos; // node -> index in topo_order
};

}  // namespace ujcore