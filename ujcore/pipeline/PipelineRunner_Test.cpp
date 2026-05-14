// Run as: bazel test ujcore/pipeline:all

#include "ujcore/pipeline/PipelineRunner.h"

#include <memory>
#include <optional>

#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/function/FunctionReturn.h"

#include "absl/status/status_matchers.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/graph/GraphTypes.h"

namespace ujcore {
namespace {

using ::ujcore::grph::EncodedData;
using ::ujcore::grph::SlotState;

class AwaitOnlyFn final : public FunctionBase {
public:
    static inline const char* uri = "/testing/await-only";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Await only")
            .WithDesc("Always returns AWAIT for testing stepPipeline.")
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new AwaitOnlyFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        return ctx.ReturnAwait("webgpu", "test-001254");
    }
};

__attribute__((constructor)) void RegisterAwaitOnlyFn() {
    FunctionRegistry::GetInstance().RegisterFunction(
        AwaitOnlyFn::uri, AwaitOnlyFn::spec, AwaitOnlyFn::newInstance, __FILE__);
}

void SetupGraphState(GraphState& graph) {
    grph::NodeInfo n1 = {
        .rawId = NodeId(1),
        .alnumid = "ABC783GH",
        .uri = "/testing/emit-float",
        .ins = {},
        .outs = {"v"},
        .inouts = {},
    };
    grph::NodeInfo n2 = {
        .rawId = NodeId(2),
        .alnumid = "DEF524MO",
        .uri = "/testing/emit-point2d",
        .ins = {},
        .outs = {"p"},
        .inouts = {},
    };
    grph::NodeInfo n3 = {
        .rawId = NodeId(3),
        .alnumid = "GHI662D7",
        .uri = "/testing/displace-point",
        .ins = {"p", "dx"},
        .outs = {"fp"},
        .inouts = {},
    };
    graph.nodeInfos = {
        {n1.rawId, n1},
        {n2.rawId, n2},
        {n3.rawId, n3},
    };


    grph::SlotInfo slot1 = {
        .parent = NodeId(1),
        .name = "v",
        .dtype = "floats",
        .access = grph::SlotInfo::AccessEnum::O,
    };
    grph::SlotInfo slot2 = {
        .parent = NodeId(2),
        .name = "p",
        .dtype = "points2d",
        .access = grph::SlotInfo::AccessEnum::O,
    };
    grph::SlotInfo slot3 = {
        .parent = NodeId(3),
        .name = "p",
        .dtype = "points2d",
        .access = grph::SlotInfo::AccessEnum::I,
    };
    grph::SlotInfo slot4 = {
        .parent = NodeId(3),
        .name = "dx",
        .dtype = "floats",
        .access = grph::SlotInfo::AccessEnum::I,
    };
    grph::SlotInfo slot5 = {
        .parent = NodeId(3),
        .name = "fp",
        .dtype = "points2d",
        .access = grph::SlotInfo::AccessEnum::O,
    };
    graph.slotInfos = {
        {{slot1.parent, slot1.name}, slot1},
        {{slot2.parent, slot2.name}, slot2},
        {{slot3.parent, slot3.name}, slot3},
        {{slot4.parent, slot4.name}, slot4},
        {{slot5.parent, slot5.name}, slot5},
    };

    SlotState slotState1 = {
        // Node 1, Slot "v"
        .inEdges = {},
        .outEdges = {EdgeId(1)},  // edge1 goes out
        .encodedData = EncodedData {
            .payload = "2.12f,5.75f",
        },
    };
    SlotState slotState2 = {
        // Node 2, Slot "p"
        .inEdges = {},
        .outEdges = {EdgeId(2)},  // edge2 goes out
        .encodedData = std::nullopt,
    };
    SlotState slotState3 = {
        // Node 3, Slot "p"
        .inEdges = {EdgeId(2)},  // edge2 comes in
        .outEdges = {},
        .encodedData = std::nullopt,
    };
    SlotState slotState4 = {
        // Node 3, Slot "dx"
        .inEdges = {EdgeId(1)},  // edge1 comes in
        .outEdges = {},
        .encodedData = std::nullopt,
    };
    SlotState slotState5 = {
        // Node 3, Slot "fp"
        .inEdges = {},
        .outEdges = {},
        .encodedData = std::nullopt,
    };

    graph.slotStates = {
        {{slot1.parent, slot1.name}, slotState1},
        {{slot2.parent, slot2.name}, slotState2},
        {{slot3.parent, slot3.name}, slotState3},
        {{slot4.parent, slot4.name}, slotState4},
        {{slot5.parent, slot5.name}, slotState5},
    };

    grph::EdgeInfo edge1 = {
        .id = EdgeId(1),
        .catid = "ABC783GH:v-GHI662D7:dx",
        .node0 = NodeId(1),
        .node1 = NodeId(3),
        .slot0 = "v",
        .slot1 = "dx",
    };
    grph::EdgeInfo edge2 = {
        .id = EdgeId(2),
        .catid = "DEF524MO:p-GHI662D7:p",
        .node0 = NodeId(2),
        .node1 = NodeId(3),
        .slot0 = "p",
        .slot1 = "p",
    };
    graph.edgeInfos = {
        {edge1.id, edge1},
        {edge2.id, edge2},
    };

    graph.nodeStates = {
        {NodeId(1), grph::NodeState{}},
        {NodeId(2), grph::NodeState{}},
        {NodeId(3), grph::NodeState{}},
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
    ABSL_EXPECT_OK(subject.RebuildFromState(graph_));
    ABSL_EXPECT_OK(subject.RunPipeline());
}

TEST_F(PipelineRunnerTest, StepPipelineSurfacesAwaitInfo) {
    GraphState graph;
    grph::NodeInfo awaitNode = {
        .rawId = NodeId(10),
        .alnumid = "AWAIT001",
        .uri = AwaitOnlyFn::uri,
        .ins = {},
        .outs = {},
        .inouts = {},
    };
    graph.nodeInfos = {{awaitNode.rawId, awaitNode}};
    graph.nodeStates = {{awaitNode.rawId, grph::NodeState{}}};
    graph.topoSortState = {.sortOrder = {awaitNode.rawId}};

    PipelineRunner subject;
    ABSL_EXPECT_OK(subject.RebuildFromState(graph));

    auto stepResultOr = subject.StepPipeline();
    ASSERT_TRUE(stepResultOr.ok());
    const flow::FlowStepResult& stepResult = stepResultOr.value();

    EXPECT_EQ(stepResult.status, flow::FlowStepResult::StatusEnum::PARTIAL);
    ASSERT_EQ(stepResult.awaitInfos.size(), 1u);
    EXPECT_EQ(stepResult.awaitInfos[0].nodeId, awaitNode.rawId);
    EXPECT_EQ(stepResult.awaitInfos[0].channel, "webgpu");
    EXPECT_EQ(stepResult.awaitInfos[0].workuri, "test-001254");
    EXPECT_TRUE(stepResult.outputs.empty());

    auto secondStepOr = subject.StepPipeline();
    ASSERT_TRUE(secondStepOr.ok());
    const flow::FlowStepResult& secondStep = secondStepOr.value();
    EXPECT_EQ(secondStep.status, flow::FlowStepResult::StatusEnum::PARTIAL);
    ASSERT_EQ(secondStep.awaitInfos.size(), 1u);
    EXPECT_EQ(secondStep.awaitInfos[0].nodeId, awaitNode.rawId);
    EXPECT_EQ(secondStep.awaitInfos[0].channel, "webgpu");
    EXPECT_EQ(secondStep.awaitInfos[0].workuri, "test-001254");
}

}  // namespace
}  // namespace ujcore
