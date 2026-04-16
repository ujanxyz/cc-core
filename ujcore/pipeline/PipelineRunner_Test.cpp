// Run as: bazel test ujcore/pipeline:all

#include "ujcore/pipeline/PipelineRunner.h"

#include <optional>

#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"

#include "absl/status/status_matchers.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/data/plinfo.h"

namespace ujcore {
namespace {

using ::ujcore::plstate::SlotDataManual;
using ::ujcore::plstate::SlotState;

void SetupGraphState(GraphState& graph) {
    plinfo::NodeInfo n1 = {
        .rawId = NodeId(1),
        .alnumid = "ABC783GH",
        .fnuri = "/testing/emit-float",
        .ins = {},
        .outs = {"v"},
        .inouts = {},
    };
    plinfo::NodeInfo n2 = {
        .rawId = NodeId(2),
        .alnumid = "DEF524MO",
        .fnuri = "/testing/emit-point2d",
        .ins = {},
        .outs = {"p"},
        .inouts = {},
    };
    plinfo::NodeInfo n3 = {
        .rawId = NodeId(3),
        .alnumid = "GHI662D7",
        .fnuri = "/testing/displace-point",
        .ins = {"p", "dx"},
        .outs = {"fp"},
        .inouts = {},
    };
    graph.node_infos = {
        {n1.rawId, n1},
        {n2.rawId, n2},
        {n3.rawId, n3},
    };


    plinfo::SlotInfo slot1 = {
        .parent = NodeId(1),
        .name = "v",
        .dtype = "float",
        .access = plinfo::SlotInfo::AccessEnum::O,
    };
    plinfo::SlotInfo slot2 = {
        .parent = NodeId(2),
        .name = "p",
        .dtype = "point2d",
        .access = plinfo::SlotInfo::AccessEnum::O,
    };
    plinfo::SlotInfo slot3 = {
        .parent = NodeId(3),
        .name = "p",
        .dtype = "point2d",
        .access = plinfo::SlotInfo::AccessEnum::I,
    };
    plinfo::SlotInfo slot4 = {
        .parent = NodeId(3),
        .name = "dx",
        .dtype = "float",
        .access = plinfo::SlotInfo::AccessEnum::I,
    };
    plinfo::SlotInfo slot5 = {
        .parent = NodeId(3),
        .name = "fp",
        .dtype = "point2d",
        .access = plinfo::SlotInfo::AccessEnum::O,
    };
    graph.slot_infos = {
        {{slot1.parent, slot1.name}, slot1},
        {{slot2.parent, slot2.name}, slot2},
        {{slot3.parent, slot3.name}, slot3},
        {{slot4.parent, slot4.name}, slot4},
        {{slot5.parent, slot5.name}, slot5},
    };

    SlotState slotState1 = {
        // Node 1, Slot "v"
        .inEdges = {},
        .outEdges = {},
        // .manual = SlotDataManual {
        //     .dtype = "float2",
        //     .encoded = "2.12f,5.75f",
        // },
        .manual = std::nullopt,
    };
    SlotState slotState2 = {
        // Node 2, Slot "p"
        .inEdges = {},
        .outEdges = {},
        .manual = std::nullopt,
    };
    SlotState slotState3 = {
        // Node 3, Slot "p"
        .inEdges = {},
        .outEdges = {},
        .manual = std::nullopt,
    };
    SlotState slotState4 = {
        // Node 3, Slot "dx"
        .inEdges = {},
        .outEdges = {},
        .manual = std::nullopt,
    };
    SlotState slotState5 = {
        // Node 3, Slot "fp"
        .inEdges = {},
        .outEdges = {},
        .manual = std::nullopt,
    };

    graph.slot_states = {
        {{slot1.parent, slot1.name}, slotState1},
        {{slot2.parent, slot2.name}, slotState2},
        {{slot3.parent, slot3.name}, slotState3},
        {{slot4.parent, slot4.name}, slotState4},
        {{slot5.parent, slot5.name}, slotState5},
    };

    plinfo::EdgeInfo edge1 = {
        .id = EdgeId(1),
        .catid = "ABC783GH:v-GHI662D7:dx",
        .node0 = NodeId(1),
        .node1 = NodeId(3),
        .slot0 = "v",
        .slot1 = "dx",
    };
    plinfo::EdgeInfo edge2 = {
        .id = EdgeId(2),
        .catid = "DEF524MO:p-GHI662D7:p",
        .node0 = NodeId(2),
        .node1 = NodeId(3),
        .slot0 = "p",
        .slot1 = "p",
    };
    graph.edge_infos = {
        {edge1.id, edge1},
        {edge2.id, edge2},
    };

    graph.topoSortState = {
        .sortOrder = {
            NodeId(1),
            NodeId(2),
            NodeId(3),
        }
    };
}

class PipelineRunnerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SetupGraphState(graph_);
    }

    GraphState graph_;
};

TEST_F(PipelineRunnerTest, Basic) {
    PipelineRunner subject;
    ABSL_ASSERT_OK(subject.BuildFromState(graph_));
    ABSL_EXPECT_OK(subject.RunPipeline());
    FAIL();
}

}  // namespace
}  // namespace ujcore
