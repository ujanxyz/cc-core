// Run as: bazel test //ujcore/graph:GraphUtils_Test
#include "ujcore/graph/GraphUtils.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/GraphBuilder.h"
#include "ujcore/graph/TopoSorter.h"
#include "ujcore/graph/IdTypes.h"

namespace ujcore {
namespace {

class GraphUtilsTest : public ::testing::Test {
 protected:
    void SetUp() override {
        topoSorter_ = std::make_unique<TopoSorter>(state_.topoSortState);
        builder_ = std::make_unique<GraphBuilder>(state_, *topoSorter_);
    }

    GraphState state_;
    std::unique_ptr<TopoSorter> topoSorter_;
    std::unique_ptr<GraphBuilder> builder_;
};

// Helper to create a simple graph for testing:
// - 2 input nodes (for float and int)
// - 2 output nodes (for float and int)
// - Slots are created automatically
void SetupSimpleGraph(GraphState& state, GraphBuilder& builder) {
    // Add input nodes
    auto r1 = builder.AddIONode("float", false, std::nullopt);
    ASSERT_TRUE(r1.ok());
    
    auto r2 = builder.AddIONode("int", false, std::nullopt);
    ASSERT_TRUE(r2.ok());
    
    // Add output nodes
    auto r3 = builder.AddIONode("float", true, std::nullopt);
    ASSERT_TRUE(r3.ok());
    
    auto r4 = builder.AddIONode("int", true, std::nullopt);
    ASSERT_TRUE(r4.ok());
}

// ============================================================================
// Tests for GetAllNodeInfos, GetAllEdgeInfos, GetAllSlotInfos
// ============================================================================

TEST_F(GraphUtilsTest, GetAllNodeInfos_ReturnsAllNodes) {
    SetupSimpleGraph(state_, *builder_);
    
    auto nodeInfos = GraphUtils::GetAllNodeInfos(state_);
    EXPECT_EQ(nodeInfos.size(), 4);
    
    // Verify that all are IO nodes
    for (const auto& nodeInfo : nodeInfos) {
        EXPECT_TRUE(nodeInfo.ntype == grph::NodeInfo::NodeType::IN || 
                    nodeInfo.ntype == grph::NodeInfo::NodeType::OUT);
    }
}

TEST_F(GraphUtilsTest, GetAllNodeInfos_Empty) {
    auto nodeInfos = GraphUtils::GetAllNodeInfos(state_);
    EXPECT_EQ(nodeInfos.size(), 0);
}

TEST_F(GraphUtilsTest, GetAllEdgeInfos_ReturnsAllEdges) {
    SetupSimpleGraph(state_, *builder_);
    
    // Add edges
    std::vector<GraphBuilder::AddEdgeEntry> entries = {
        {.node0 = NodeId(1), .slot0 = "$out", .node1 = NodeId(3), .slot1 = "$in"},
        {.node0 = NodeId(2), .slot0 = "$out", .node1 = NodeId(4), .slot1 = "$in"},
    };
    auto edgesResult = builder_->AddEdges(entries);
    ASSERT_TRUE(edgesResult.ok());
    
    auto edgeInfos = GraphUtils::GetAllEdgeInfos(state_);
    EXPECT_EQ(edgeInfos.size(), 2);
}

TEST_F(GraphUtilsTest, GetAllEdgeInfos_Empty) {
    auto edgeInfos = GraphUtils::GetAllEdgeInfos(state_);
    EXPECT_EQ(edgeInfos.size(), 0);
}

TEST_F(GraphUtilsTest, GetAllSlotInfos_ReturnsAllSlots) {
    SetupSimpleGraph(state_, *builder_);
    
    auto slotInfos = GraphUtils::GetAllSlotInfos(state_);
    // 4 nodes × 1 slot each = 4 slots
    EXPECT_EQ(slotInfos.size(), 4);
    
    // Verify slot names and access patterns
    std::set<std::string> slotNames;
    for (const auto& slotInfo : slotInfos) {
        slotNames.insert(slotInfo.name);
    }
    // All IO nodes have a single slot named "$out" (input) or "$in" (output)
    EXPECT_TRUE(slotNames.contains("$out") || slotNames.contains("$in"));
}

TEST_F(GraphUtilsTest, GetAllSlotInfos_Empty) {
    auto slotInfos = GraphUtils::GetAllSlotInfos(state_);
    EXPECT_EQ(slotInfos.size(), 0);
}

// ============================================================================
// Tests for LookupNodeStates
// ============================================================================

TEST_F(GraphUtilsTest, LookupNodeStates_Success) {
    SetupSimpleGraph(state_, *builder_);
    
    std::vector<NodeId> nodeIds = {NodeId(1), NodeId(2)};
    auto result = GraphUtils::LookupNodeStates(state_, nodeIds);
    ASSERT_TRUE(result.ok());
    
    auto states = result.value();
    EXPECT_EQ(states.size(), 2);
    EXPECT_TRUE(states.contains(NodeId(1)));
    EXPECT_TRUE(states.contains(NodeId(2)));
}

TEST_F(GraphUtilsTest, LookupNodeStates_NotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    std::vector<NodeId> nodeIds = {NodeId(1), NodeId(999)};  // 999 doesn't exist
    auto result = GraphUtils::LookupNodeStates(state_, nodeIds);
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

TEST_F(GraphUtilsTest, LookupNodeStates_Empty) {
    std::vector<NodeId> nodeIds;
    auto result = GraphUtils::LookupNodeStates(state_, nodeIds);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 0);
}

// ============================================================================
// Tests for LookupSlotStates
// ============================================================================

TEST_F(GraphUtilsTest, LookupSlotStates_Success) {
    SetupSimpleGraph(state_, *builder_);
    
    std::vector<SlotId> slotIds = {
        SlotId{NodeId(1), "$out"},
        SlotId{NodeId(3), "$in"}
    };
    auto result = GraphUtils::LookupSlotStates(state_, slotIds);
    ASSERT_TRUE(result.ok());
    
    auto states = result.value();
    EXPECT_EQ(states.size(), 2);
}

TEST_F(GraphUtilsTest, LookupSlotStates_NotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    std::vector<SlotId> slotIds = {
        SlotId{NodeId(1), "$out"},
        SlotId{NodeId(999), "nonexistent"}
    };
    auto result = GraphUtils::LookupSlotStates(state_, slotIds);
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

TEST_F(GraphUtilsTest, LookupSlotStates_Empty) {
    std::vector<SlotId> slotIds;
    auto result = GraphUtils::LookupSlotStates(state_, slotIds);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), 0);
}

// ============================================================================
// Tests for LookupIoSlot (const)
// ============================================================================

TEST_F(GraphUtilsTest, LookupIoSlot_InputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    // NodeId(1) should be an input node
    auto result = GraphUtils::LookupIoSlot(state_, NodeId(1));
    ASSERT_TRUE(result.ok());
    
    auto [slotInfo, slotState] = result.value();
    EXPECT_NE(slotInfo, nullptr);
    EXPECT_NE(slotState, nullptr);
    EXPECT_EQ(slotInfo->name, "$out");
    EXPECT_EQ(slotInfo->access, grph::SlotInfo::AccessEnum::O);
}

TEST_F(GraphUtilsTest, LookupIoSlot_OutputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    // NodeId(3) should be an output node
    auto result = GraphUtils::LookupIoSlot(state_, NodeId(3));
    ASSERT_TRUE(result.ok());
    
    auto [slotInfo, slotState] = result.value();
    EXPECT_NE(slotInfo, nullptr);
    EXPECT_NE(slotState, nullptr);
    EXPECT_EQ(slotInfo->name, "$in");
    EXPECT_EQ(slotInfo->access, grph::SlotInfo::AccessEnum::I);
}

TEST_F(GraphUtilsTest, LookupIoSlot_NodeNotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    auto result = GraphUtils::LookupIoSlot(state_, NodeId(999));
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

// ============================================================================
// Tests for LookupMutableIoSlot
// ============================================================================

TEST_F(GraphUtilsTest, LookupMutableIoSlot_InputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    // NodeId(1) should be an input node
    auto result = GraphUtils::LookupMutableIoSlot(state_, NodeId(1));
    ASSERT_TRUE(result.ok());
    
    auto [slotInfo, slotState] = result.value();
    EXPECT_NE(slotInfo, nullptr);
    EXPECT_NE(slotState, nullptr);
    EXPECT_EQ(slotInfo->name, "$out");
    
    // Verify we can modify the slot state
    auto& mutableState = *const_cast<grph::SlotState*>(slotState);
    mutableState.genId = 42;
    EXPECT_EQ(mutableState.genId, 42);
}

TEST_F(GraphUtilsTest, LookupMutableIoSlot_OutputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    // NodeId(3) should be an output node
    auto result = GraphUtils::LookupMutableIoSlot(state_, NodeId(3));
    ASSERT_TRUE(result.ok());
    
    auto [slotInfo, slotState] = result.value();
    EXPECT_NE(slotInfo, nullptr);
    EXPECT_NE(slotState, nullptr);
    EXPECT_EQ(slotInfo->name, "$in");
}

TEST_F(GraphUtilsTest, LookupMutableIoSlot_NodeNotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    auto result = GraphUtils::LookupMutableIoSlot(state_, NodeId(999));
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

// ============================================================================
// Tests for CopyNodeState
// ============================================================================

TEST_F(GraphUtilsTest, CopyNodeState_Success) {
    SetupSimpleGraph(state_, *builder_);
    
    auto nodeState = GraphUtils::CopyNodeState(state_, NodeId(1));
    EXPECT_TRUE(nodeState.has_value());
}

TEST_F(GraphUtilsTest, CopyNodeState_NotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    auto nodeState = GraphUtils::CopyNodeState(state_, NodeId(999));
    EXPECT_FALSE(nodeState.has_value());
}

TEST_F(GraphUtilsTest, CopyNodeState_ReturnsIndependentCopy) {
    SetupSimpleGraph(state_, *builder_);
    
    auto nodeState1 = GraphUtils::CopyNodeState(state_, NodeId(1));
    EXPECT_TRUE(nodeState1.has_value());
    
    // Modify the original state
    auto iter = state_.nodeStates.find(NodeId(1));
    ASSERT_NE(iter, state_.nodeStates.end());
    iter->second.label = "modified";
    
    // The copy should still have the old value
    EXPECT_NE(nodeState1.value().label, "modified");
}

// ============================================================================
// Tests for CopyAllSlotInfos
// ============================================================================

TEST_F(GraphUtilsTest, CopyAllSlotInfos_InputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    auto slotInfos = GraphUtils::CopyAllSlotInfos(state_, NodeId(1));
    // Input node has one output slot
    EXPECT_EQ(slotInfos.size(), 1);
    EXPECT_EQ(slotInfos[0].name, "$out");
    EXPECT_EQ(slotInfos[0].access, grph::SlotInfo::AccessEnum::O);
}

TEST_F(GraphUtilsTest, CopyAllSlotInfos_OutputNode) {
    SetupSimpleGraph(state_, *builder_);
    
    auto slotInfos = GraphUtils::CopyAllSlotInfos(state_, NodeId(3));
    // Output node has one input slot
    EXPECT_EQ(slotInfos.size(), 1);
    EXPECT_EQ(slotInfos[0].name, "$in");
    EXPECT_EQ(slotInfos[0].access, grph::SlotInfo::AccessEnum::I);
}

TEST_F(GraphUtilsTest, CopyAllSlotInfos_NodeNotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    auto slotInfos = GraphUtils::CopyAllSlotInfos(state_, NodeId(999));
    EXPECT_EQ(slotInfos.size(), 0);
}

// ============================================================================
// Tests for GetDownstreamNodeIds
// ============================================================================

TEST_F(GraphUtilsTest, GetDownstreamNodeIds_WithEdges) {
    SetupSimpleGraph(state_, *builder_);
    
    // Connect input node to output node
    std::vector<GraphBuilder::AddEdgeEntry> entries = {
        {.node0 = NodeId(1), .slot0 = "$out", .node1 = NodeId(3), .slot1 = "$in"},
    };
    auto edgesResult = builder_->AddEdges(entries);
    ASSERT_TRUE(edgesResult.ok());
    
    auto downstreamNodes = GraphUtils::GetDownstreamNodeIds(state_, NodeId(1));
    EXPECT_EQ(downstreamNodes.size(), 1);
    EXPECT_TRUE(downstreamNodes.contains(NodeId(3)));
}

TEST_F(GraphUtilsTest, GetDownstreamNodeIds_MultipleDownstream) {
    // Create a more complex graph
    auto result1 = builder_->AddIONode("float", false, std::nullopt);  // Node 1
    auto result2 = builder_->AddIONode("float", true, std::nullopt);   // Node 2
    auto result3 = builder_->AddIONode("float", true, std::nullopt);   // Node 3
    
    ASSERT_TRUE(result1.ok());
    ASSERT_TRUE(result2.ok());
    ASSERT_TRUE(result3.ok());
    
    // Connect node 1 to both node 2 and node 3
    std::vector<GraphBuilder::AddEdgeEntry> entries = {
        {.node0 = NodeId(1), .slot0 = "$out", .node1 = NodeId(2), .slot1 = "$in"},
        {.node0 = NodeId(1), .slot0 = "$out", .node1 = NodeId(3), .slot1 = "$in"},
    };
    auto edgesResult = builder_->AddEdges(entries);
    ASSERT_TRUE(edgesResult.ok());
    
    auto downstreamNodes = GraphUtils::GetDownstreamNodeIds(state_, NodeId(1));
    EXPECT_EQ(downstreamNodes.size(), 2);
    EXPECT_TRUE(downstreamNodes.contains(NodeId(2)));
    EXPECT_TRUE(downstreamNodes.contains(NodeId(3)));
}

TEST_F(GraphUtilsTest, GetDownstreamNodeIds_NoDownstream) {
    SetupSimpleGraph(state_, *builder_);
    
    // Node with no outgoing edges
    auto downstreamNodes = GraphUtils::GetDownstreamNodeIds(state_, NodeId(3));
    EXPECT_EQ(downstreamNodes.size(), 0);
}

TEST_F(GraphUtilsTest, GetDownstreamNodeIds_NodeNotFound) {
    SetupSimpleGraph(state_, *builder_);
    
    auto downstreamNodes = GraphUtils::GetDownstreamNodeIds(state_, NodeId(999));
    EXPECT_EQ(downstreamNodes.size(), 0);
}

}  // namespace
}  // namespace ujcore
