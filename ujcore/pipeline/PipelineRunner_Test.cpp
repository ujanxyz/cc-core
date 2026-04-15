// Run as: bazel test ujcore/pipeline:all

#include "ujcore/data/IdTypes.h"
#include "ujcore/pipeline/PipelineRunner.h"

#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/data/plinfo.h"

namespace ujcore {
namespace {

void SetupGraphState(GraphState& graph) {
    plinfo::NodeInfo n1 = {
        .rawId = NodeId(1),
        .alnumid = "ABC783GH",
        .fnuri = "/util/myfn",
        .ins = {"x"},
        .outs = {"fx"},
        .inouts = {},
    };

    plinfo::SlotInfo slot1 = {
        .parent = NodeId(1),
        .name = "x",
        .dtype = "float2",
        .access = plinfo::SlotInfo::AccessEnum::I,
    };

    plinfo::SlotInfo slot2 = {
        .parent = NodeId(1),
        .name = "fx",
        .dtype = "float2",
        .access = plinfo::SlotInfo::AccessEnum::O,
    };

    graph.node_infos = {
        {n1.rawId, n1},
    };

    graph.slot_infos = {
        {{slot1.parent, slot1.name}, slot1},
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
    subject.BuildFromState(graph_);
}

}  // namespace
}  // namespace ujcore
