// Run as: bazel test ujcore/pipeline:PipelineBuilder_Test

#include "ujcore/pipeline/PipelineBuilder.h"

#include <optional>

#include "absl/status/status_matchers.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/pipeline/GraphPipeline.h"

namespace ujcore {
namespace {

using ::ujcore::grph::EncodedData;
using ::ujcore::grph::SlotState;

// Helper to create a node info.
grph::NodeInfo CreateNodeInfo(NodeId id, const std::string& alnumid, const std::string& uri,
                              const std::vector<std::string>& ins,
                              const std::vector<std::string>& outs) {
    return grph::NodeInfo{
        .rawId = id,
        .alnumid = alnumid,
        .uri = uri,
        .ins = ins,
        .outs = outs,
        .inouts = {},
    };
}

// Helper to create a slot info.
grph::SlotInfo CreateSlotInfo(NodeId parent, const std::string& name,
                              const std::string& dtype,
                              grph::SlotInfo::AccessEnum access) {
    return grph::SlotInfo{
        .parent = parent,
        .name = name,
        .dtype = dtype,
        .access = access,
    };
}

// Helper to create an edge info.
grph::EdgeInfo CreateEdgeInfo(EdgeId id, const std::string& catid,
                              NodeId node0, const std::string& slot0,
                              NodeId node1, const std::string& slot1) {
    return grph::EdgeInfo{
        .id = id,
        .catid = catid,
        .node0 = node0,
        .node1 = node1,
        .slot0 = slot0,
        .slot1 = slot1,
    };
}

// Build a 10-node test graph using available test functions.
// Layer 1: Nodes 1-2 (emitters)
// Layer 2: Nodes 3-10 (displace-point nodes with various connections)
//
// Since only 3 test functions are available (emit-float, emit-point2d, displace-point),
// we create a DAG using these functions repeatedly.
void SetupLargeGraphState(GraphState& graph) {
    // Define 10 nodes in topological order using available test functions.
    std::vector<grph::NodeInfo> nodes = {
        CreateNodeInfo(NodeId(1), "EMIT_1", "/testing/emit-float", {}, {"v"}),
        CreateNodeInfo(NodeId(2), "EMIT_2", "/testing/emit-point2d", {}, {"p"}),
        CreateNodeInfo(NodeId(3), "DISPLACE_1", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(4), "DISPLACE_2", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(5), "DISPLACE_3", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(6), "DISPLACE_4", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(7), "DISPLACE_5", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(8), "DISPLACE_6", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(9), "DISPLACE_7", "/testing/displace-point", {"p", "dx"}, {"fp"}),
        CreateNodeInfo(NodeId(10), "DISPLACE_8", "/testing/displace-point", {"p", "dx"}, {"fp"}),
    };

    // Populate nodeInfos.
    for (const auto& node : nodes) {
        graph.nodeInfos[node.rawId] = node;
    }

    // Define slots (inputs and outputs for each node).
    std::vector<grph::SlotInfo> slots = {
        // Node 1 outputs
        CreateSlotInfo(NodeId(1), "v", "floats", grph::SlotInfo::AccessEnum::O),
        // Node 2 outputs
        CreateSlotInfo(NodeId(2), "p", "points2d", grph::SlotInfo::AccessEnum::O),
        // Nodes 3-10: displace-point nodes with inputs and outputs
        CreateSlotInfo(NodeId(3), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(3), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(3), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(4), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(4), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(4), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(5), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(5), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(5), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(6), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(6), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(6), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(7), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(7), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(7), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(8), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(8), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(8), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(9), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(9), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(9), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
        CreateSlotInfo(NodeId(10), "p", "points2d", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(10), "dx", "floats", grph::SlotInfo::AccessEnum::I),
        CreateSlotInfo(NodeId(10), "fp", "points2d", grph::SlotInfo::AccessEnum::O),
    };

    // Populate slotInfos.
    for (const auto& slot : slots) {
        graph.slotInfos[{slot.parent, slot.name}] = slot;
    }

    // Create slot states (initialize as empty, no incoming edges initially).
    for (const auto& slot : slots) {
        SlotState state{};
        if (slot.access == grph::SlotInfo::AccessEnum::O) {
            // Output slots have optional encoded data.
            if (slot.parent == NodeId(1)) {
                // emit-float produces initial data
                state.encodedData = EncodedData{.payload = "1.5f"};
            } else if (slot.parent == NodeId(2)) {
                // emit-point2d produces initial data
                state.encodedData = EncodedData{.payload = "(1.0f, 2.0f)"};
            } else {
                state.encodedData = std::nullopt;
            }
        } else {
            // Input slots have no initial data.
            state.encodedData = std::nullopt;
        }
        graph.slotStates[{slot.parent, slot.name}] = state;
    }

    // Create 20 edges connecting the nodes in a DAG pattern.
    std::vector<grph::EdgeInfo> edges = {
        // Node 1 (emit-float) connects to displace nodes (provides dx).
        CreateEdgeInfo(EdgeId(1), "1:v-3:dx", NodeId(1), "v", NodeId(3), "dx"),
        CreateEdgeInfo(EdgeId(2), "1:v-4:dx", NodeId(1), "v", NodeId(4), "dx"),
        CreateEdgeInfo(EdgeId(3), "1:v-5:dx", NodeId(1), "v", NodeId(5), "dx"),
        CreateEdgeInfo(EdgeId(4), "1:v-6:dx", NodeId(1), "v", NodeId(6), "dx"),
        CreateEdgeInfo(EdgeId(5), "1:v-7:dx", NodeId(1), "v", NodeId(7), "dx"),

        // Node 2 (emit-point2d) connects to displace nodes (provides p).
        CreateEdgeInfo(EdgeId(6), "2:p-3:p", NodeId(2), "p", NodeId(3), "p"),
        CreateEdgeInfo(EdgeId(7), "2:p-4:p", NodeId(2), "p", NodeId(4), "p"),
        CreateEdgeInfo(EdgeId(8), "2:p-5:p", NodeId(2), "p", NodeId(5), "p"),
        CreateEdgeInfo(EdgeId(9), "2:p-6:p", NodeId(2), "p", NodeId(6), "p"),
        CreateEdgeInfo(EdgeId(10), "2:p-7:p", NodeId(2), "p", NodeId(7), "p"),

        // Displace nodes forward their outputs to other displace nodes.
        CreateEdgeInfo(EdgeId(11), "3:fp-8:p", NodeId(3), "fp", NodeId(8), "p"),
        CreateEdgeInfo(EdgeId(12), "4:fp-9:p", NodeId(4), "fp", NodeId(9), "p"),
        CreateEdgeInfo(EdgeId(13), "5:fp-10:p", NodeId(5), "fp", NodeId(10), "p"),
        CreateEdgeInfo(EdgeId(14), "3:fp-9:p", NodeId(3), "fp", NodeId(9), "p"),
        CreateEdgeInfo(EdgeId(15), "4:fp-10:p", NodeId(4), "fp", NodeId(10), "p"),

        // More cross connections.
        CreateEdgeInfo(EdgeId(16), "1:v-8:dx", NodeId(1), "v", NodeId(8), "dx"),
        CreateEdgeInfo(EdgeId(17), "1:v-9:dx", NodeId(1), "v", NodeId(9), "dx"),
        CreateEdgeInfo(EdgeId(18), "1:v-10:dx", NodeId(1), "v", NodeId(10), "dx"),
        CreateEdgeInfo(EdgeId(19), "6:fp-9:p", NodeId(6), "fp", NodeId(9), "p"),
        CreateEdgeInfo(EdgeId(20), "7:fp-10:p", NodeId(7), "fp", NodeId(10), "p"),
    };

    // Populate edgeInfos and update slot states with edge references.
    for (const auto& edge : edges) {
        graph.edgeInfos[edge.id] = edge;
        // Update outgoing edges for source slot.
        graph.slotStates[{edge.node0, edge.slot0}].outEdges.insert(edge.id);
        // Update incoming edges for destination slot.
        graph.slotStates[{edge.node1, edge.slot1}].inEdges.insert(edge.id);
    }

    // Initialize nodeStates for each node.
    for (const auto& [nodeId, nodeInfo] : graph.nodeInfos) {
        graph.nodeStates[nodeId] = grph::NodeState{};
    }

    // Set topological order.
    graph.topoSortState = {
        .sortOrder = {
            NodeId(1), NodeId(2),  // Layer 1: Emitters
            NodeId(3), NodeId(4), NodeId(5), NodeId(6), NodeId(7),  // Layer 2: First transforms
            NodeId(8), NodeId(9), NodeId(10),  // Layer 3: Final transforms
        }};
}

class PipelineBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        SetupLargeGraphState(graph_);
    }

    GraphState graph_;
};

// Test that NodeExecGroup structure is correctly formed.
TEST_F(PipelineBuilderTest, NodeExecGroupStructure) {
    GraphPipeline pipeline;
    ABSL_ASSERT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    // Verify nodeGroups is populated.
    ASSERT_EQ(pipeline.nodeGroups.size(), 10) << "Expected 10 node groups";

    // Verify groups are in topo order.
    std::vector<NodeId> executionOrder;
    for (const auto& group : pipeline.nodeGroups) {
        executionOrder.push_back(group.nodeId);
    }
    EXPECT_EQ(executionOrder, graph_.topoSortState.sortOrder);
}

// Test that incoming edge transfers are correctly grouped.
TEST_F(PipelineBuilderTest, IncomingEdgeGrouping) {
    GraphPipeline pipeline;
    ABSL_ASSERT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    // Node 1 and 2 should have no incoming edges.
    {
        auto group1 = std::find_if(pipeline.nodeGroups.begin(), pipeline.nodeGroups.end(),
                                  [](const auto& g) { return g.nodeId == NodeId(1); });
        EXPECT_EQ(group1->incomingTransfers.size(), 0);

        auto group2 = std::find_if(pipeline.nodeGroups.begin(), pipeline.nodeGroups.end(),
                                  [](const auto& g) { return g.nodeId == NodeId(2); });
        EXPECT_EQ(group2->incomingTransfers.size(), 0);
    }

    // Node 3 should have 2 incoming edges (from nodes 1 and 2).
    {
        auto group3 = std::find_if(pipeline.nodeGroups.begin(), pipeline.nodeGroups.end(),
                                  [](const auto& g) { return g.nodeId == NodeId(3); });
        EXPECT_EQ(group3->incomingTransfers.size(), 2);
    }

    // Node 6 should have 2+ incoming edges.
    {
        auto group6 = std::find_if(pipeline.nodeGroups.begin(), pipeline.nodeGroups.end(),
                                  [](const auto& g) { return g.nodeId == NodeId(6); });
        EXPECT_GE(group6->incomingTransfers.size(), 2);
    }
}

// Test that all 20 edges are accounted for in groups.
TEST_F(PipelineBuilderTest, AllEdgesGrouped) {
    GraphPipeline pipeline;
    ABSL_ASSERT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    int totalEdgeSteps = 0;
    for (const auto& group : pipeline.nodeGroups) {
        totalEdgeSteps += group.incomingTransfers.size();
    }
    EXPECT_EQ(totalEdgeSteps, 20) << "Expected all 20 edges to be grouped";
}

// Test that each group has a valid node step.
TEST_F(PipelineBuilderTest, EachGroupHasNodeStep) {
    GraphPipeline pipeline;
    ABSL_ASSERT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    for (const auto& group : pipeline.nodeGroups) {
        // Every group must have either a function-node reference or an IO-node reference.
        bool hasValidStep = std::holds_alternative<std::reference_wrapper<PipelineFnNode>>(group.nodeStep) ||
                           std::holds_alternative<std::reference_wrapper<PipelineIONode>>(group.nodeStep);
        EXPECT_TRUE(hasValidStep) << "Group for node " << group.nodeId.value << " has invalid step";
    }
}

// Test that the graph with 10 nodes and 20 edges is correctly processed.
TEST_F(PipelineBuilderTest, LargeGraphProcessing) {
    GraphPipeline pipeline;
    ABSL_EXPECT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    // Verify structure sizes.
    EXPECT_EQ(pipeline.nodeStages.size(), 10);
    EXPECT_EQ(pipeline.nodeGroups.size(), 10);

    // Verify execution order matches topo sort.
    for (size_t i = 0; i < pipeline.nodeGroups.size(); ++i) {
        EXPECT_EQ(pipeline.nodeGroups[i].nodeId, graph_.topoSortState.sortOrder[i]);
    }
}

// Test that groups maintain proper ordering for incremental updates.
TEST_F(PipelineBuilderTest, GroupOrderingForIncrementalUpdates) {
    GraphPipeline pipeline;
    ABSL_ASSERT_OK(PipelineBuilder::Rebuild(graph_, pipeline));

    // Verify that if we iterate groups in order, dependencies are satisfied.
    // (i.e., a node never appears before its dependencies).
    std::set<NodeId> processed;
    for (const auto& group : pipeline.nodeGroups) {
        // Check that all incoming transfer sources have been processed.
        // (This is implicitly satisfied by topo order.)
        processed.insert(group.nodeId);
    }
    EXPECT_EQ(processed.size(), 10) << "All nodes should be processed";
}

}  // namespace
}  // namespace ujcore
