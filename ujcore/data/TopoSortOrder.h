#pragma once

#include <map>
#include <set>
#include <vector>

#include "ujcore/data/IdTypes.h"
#include "ujcore/data/TopoSortState.h"

namespace ujcore {

class TopoSortOrder final {
 public:
    TopoSortOrder(TopoSortState& state);

    void AddNode(const NodeId& u);
    void RemoveNode(const NodeId& u);
    void RemoveEdge(const NodeId& u, const NodeId& v);
    bool AddEdge(const NodeId& u, const NodeId& v);

    void PrintTopoOrder();

    const std::vector<NodeId>& CurrentOrder() const {
        return state_.sortOrder;
    }

 private:
    bool reorder(const NodeId& u, const NodeId& v);
    bool forward_dfs(const NodeId& curr, int upper_bound,
                         std::vector<int>& delta_f_indices);
    bool backward_dfs(const NodeId& curr, int lower_bound, 
                         std::vector<int>& delta_b_indices);

    // The state must outlive or have same lifetime as this sorter.
    TopoSortState& state_;

    std::map<NodeId, int> pos; // node -> index in topo_order
    std::map<NodeId, std::set<NodeId>> inAdj;
    std::map<NodeId, std::set<NodeId>> outAdj;

    friend class TopoSortOrderTest;
};

}  // namespace ujcore