#include "ujcore/data/TopoSorter.h"

#include <map>
#include <set>

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/data/IdTypes.h"

namespace ujcore {

// Helper to check if the current topo_order is valid based on adj
class TopoSorterTest : public ::testing::Test {
 protected:
    TopoSorterTest() : sorter(state) {}
 
    bool IsValidTopoOrder() {
        const std::vector<NodeId>& order = state.sortOrder;
        const std::map<NodeId, std::set<NodeId>>& inAdj = sorter.inAdj;
        const std::map<NodeId, std::set<NodeId>>& outAdj = sorter.outAdj;
        std::map<NodeId, int> pos;
        for (int i = 0; i < order.size(); ++i) {
            pos[order[i]] = i;
            LOG(INFO) << order[i].value;
        }
        for (auto const& [v, upstream_nodes] : inAdj) {
            for (const auto& u : upstream_nodes) {
                if (pos[u] >= pos[v]) return false;
            }
        }
        for (auto const& [u, downstream_nodes] : outAdj) {
            for (const auto& v : downstream_nodes) {
                if (pos[u] >= pos[v]) return false;
            }
        }
        return true;
    }

    TopoSortState state;
    TopoSorter sorter;
};

namespace {

// 1. Basic Sequential Insertion
TEST_F(TopoSorterTest, SimplePath) {
    NodeId A(1);
    NodeId B(2);
    NodeId C(3);

    sorter.AddEdge(A, B);
    sorter.AddEdge(B, C);
    
    // Path: A -> B -> C. Order must be A, B, C.
    // We don't need to check pos[u] to print/verify order.
    // But we use the logic to ensure the vector matches.
    sorter.PrintTopoOrder();
    ASSERT_TRUE(IsValidTopoOrder());
}

// 2. Backward Edge Triggering Reorder
TEST_F(TopoSorterTest, ReorderOnBackwardEdge) {
    NodeId A(1);
    NodeId B(2);
    NodeId C(3);

    sorter.AddNode(A);
    sorter.AddNode(B);
    sorter.AddNode(C);
    // Initial: A, B, C (order of addition)
    
    // Add C -> A. This is a "backward" edge relative to the initial list.
    // The algorithm must shift C before A.
    bool success = sorter.AddEdge(C, A);
    
    ASSERT_TRUE(success);
    ASSERT_TRUE(IsValidTopoOrder());
    // New valid order should be C, A, B or C, B, A etc.
    // The Pearce-Kelly logic specifically moves C before A.
}

// 3. Cycle Detection (Crucial Edge Case)
TEST_F(TopoSorterTest, PreventCycle) {
    NodeId A(1);
    NodeId B(2);
    NodeId C(3);

    sorter.AddEdge(A, B);
    sorter.AddEdge(B, C);
    
    // Attempting to add C -> A should fail and return false
    bool success = sorter.AddEdge(C, A);
    
    EXPECT_FALSE(success);
    // The order should remain A -> B -> C
}

// 4. Node Removal and Cascading Edge Cleanup
TEST_F(TopoSorterTest, RemoveNodeAndEdges) {
    NodeId A(1);
    NodeId B(2);
    NodeId C(3);

    sorter.AddEdge(A, B);
    sorter.AddEdge(B, C);
    sorter.AddEdge(A, C);

    sorter.RemoveNode(B);
    
    // After removing B, the edge A->B and B->C should be gone.
    // The edge A->C should still exist.
    // Total nodes remaining: 2 (A and C)
    ASSERT_TRUE(IsValidTopoOrder());
}

// 5. Disconnected Components
TEST_F(TopoSorterTest, DisconnectedNodes) {
    NodeId A(1);
    NodeId B(2);
    NodeId Z(10);

    sorter.AddNode(Z); // Independent node
    sorter.AddEdge(A, B);
    
    // Z could be anywhere, but A must be before B.
    // The implementation adds Z first, so Z usually stays at index 0.
    ASSERT_TRUE(IsValidTopoOrder());
}

// 6. Redundant Edge Addition
TEST_F(TopoSorterTest, RedundantEdge) {
    NodeId A(1);
    NodeId B(2);

    sorter.AddEdge(A, B);
    bool first = sorter.AddEdge(A, B);
    bool second = sorter.AddEdge(A, B);
    
    EXPECT_TRUE(first);
    EXPECT_TRUE(second); // Adding the same edge shouldn't break anything.
}

// 7. Large Reorder (Pearce-Kelly Logic)
TEST_F(TopoSorterTest, ComplexReorder) {
    // Current: 1, 2, 3, 4, 5
    for(int i = 1; i <= 5; ++i) {
        sorter.AddNode(NodeId(i));
    }
    
    // Add edge 5 -> 1. 
    // This forces the "Affected Region" to include the whole graph.
    ASSERT_TRUE(sorter.AddEdge(NodeId(5), NodeId(1)));
    ASSERT_TRUE(IsValidTopoOrder());
    // Order should now have "5" appearing before "1".
}

} // namespace
}  // namespace ujcore
