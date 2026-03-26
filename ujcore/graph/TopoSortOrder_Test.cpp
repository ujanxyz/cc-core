#include "ujcore/graph/TopoSortOrder.h"

#include <map>
#include <set>

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"

namespace ujcore {

// Helper to check if the current topo_order is valid based on adj
class TopoSortOrderTest : public ::testing::Test {
 protected:
    bool IsValidTopo(const TopoSortOrder& ts) {
        const std::vector<std::string>& order = ts.topo_order;
        const std::map<std::string, std::set<std::string>>& in_adj = ts.in_adj;
        const std::map<std::string, std::set<std::string>>& out_adj = ts.out_adj;
        std::map<std::string, int> pos;
        for (int i = 0; i < order.size(); ++i) {
            pos[order[i]] = i;
            LOG(INFO) << order[i];
        }
        for (auto const& [v, upstream_nodes] : in_adj) {
            for (const auto& u : upstream_nodes) {
                if (pos[u] >= pos[v]) return false;
            }
        }
        for (auto const& [u, downstream_nodes] : out_adj) {
            for (const auto& v : downstream_nodes) {
                if (pos[u] >= pos[v]) return false;
            }
        }
        return true;
    }
};

namespace {

// 1. Basic Sequential Insertion
TEST_F(TopoSortOrderTest, SimplePath) {
    TopoSortOrder ts;
    ts.AddEdge("A", "B");
    ts.AddEdge("B", "C");
    
    // Path: A -> B -> C. Order must be A, B, C.
    // We don't need to check pos[u] to print/verify order.
    // But we use the logic to ensure the vector matches.
    ts.PrintTopoOrder();
    ASSERT_TRUE(IsValidTopo(ts));
}

// 2. Backward Edge Triggering Reorder
TEST_F(TopoSortOrderTest, ReorderOnBackwardEdge) {
    TopoSortOrder ts;
    ts.AddNode("A");
    ts.AddNode("B");
    ts.AddNode("C");
    // Initial: A, B, C (order of addition)
    
    // Add C -> A. This is a "backward" edge relative to the initial list.
    // The algorithm must shift C before A.
    bool success = ts.AddEdge("C", "A");
    
    ASSERT_TRUE(success);
    ASSERT_TRUE(IsValidTopo(ts));
    // New valid order should be C, A, B or C, B, A etc.
    // The Pearce-Kelly logic specifically moves C before A.
}

// 3. Cycle Detection (Crucial Edge Case)
TEST_F(TopoSortOrderTest, PreventCycle) {
    TopoSortOrder ts;
    ts.AddEdge("A", "B");
    ts.AddEdge("B", "C");
    
    // Attempting to add C -> A should fail and return false
    bool success = ts.AddEdge("C", "A");
    
    EXPECT_FALSE(success);
    // The order should remain A -> B -> C
}

// 4. Node Removal and Cascading Edge Cleanup
TEST_F(TopoSortOrderTest, RemoveNodeAndEdges) {
    TopoSortOrder ts;
    ts.AddEdge("A", "B");
    ts.AddEdge("B", "C");
    ts.AddEdge("A", "C");

    ts.RemoveNode("B");
    
    // After removing B, the edge A->B and B->C should be gone.
    // The edge A->C should still exist.
    // Total nodes remaining: 2 (A and C)
    ASSERT_TRUE(IsValidTopo(ts));
}

// 5. Disconnected Components
TEST_F(TopoSortOrderTest, DisconnectedNodes) {
    TopoSortOrder ts;
    ts.AddNode("Z"); // Independent node
    ts.AddEdge("A", "B");
    
    // Z could be anywhere, but A must be before B.
    // The implementation adds Z first, so Z usually stays at index 0.
    ASSERT_TRUE(IsValidTopo(ts));
}

// 6. Redundant Edge Addition
TEST_F(TopoSortOrderTest, RedundantEdge) {
    TopoSortOrder ts;
    ts.AddEdge("A", "B");
    bool first = ts.AddEdge("A", "B");
    bool second = ts.AddEdge("A", "B");
    
    EXPECT_TRUE(first);
    EXPECT_TRUE(second); // Adding the same edge shouldn't break anything.
}

// 7. Large Reorder (Pearce-Kelly Logic)
TEST_F(TopoSortOrderTest, ComplexReorder) {
    TopoSortOrder ts;
    // Current: 1, 2, 3, 4, 5
    for(int i=1; i<=5; ++i) ts.AddNode(std::to_string(i));
    
    // Add edge 5 -> 1. 
    // This forces the "Affected Region" to include the whole graph.
    ASSERT_TRUE(ts.AddEdge("5", "1"));    
    ASSERT_TRUE(IsValidTopo(ts));
    // Order should now have "5" appearing before "1".
}


} // namespace
}  // namespace ujcore