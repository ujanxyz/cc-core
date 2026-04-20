#include "ujcore/pipeline/PipelineRunner.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/pipeline/PipelineBuilder.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using NodeRunStep = GraphPipeline::NodeRunStep;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using ManualDataStep = GraphPipeline::ManualDataStep;

}  // namespace

absl::Status PipelineRunner::BuildFromState(const GraphState& state) {

    PipelineBuilder builder(state, pipeline_);
    RETURN_IF_ERROR(builder.Rebuild());
    return absl::OkStatus();
}

absl::StatusOr<plstate::GraphRunResult> PipelineRunner::RunPipeline() {
    plstate::GraphRunResult result;
    for (auto& step : pipeline_.execSteps) {
        if (std::holds_alternative<EdgePropagateStep>(step)) {
            const auto& edgeStep = std::get<EdgePropagateStep>(step);
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
        }
        else if (std::holds_alternative<NodeRunStep>(step)) {
            const NodeRunStep& nodeStep = std::get<NodeRunStep>(step);
            ASSIGN_OR_RETURN(bool runStatus, nodeStep.fnNode->RunFunction());
            if (!runStatus) {
                return absl::InternalError("Node run failed");
            }
        }
        else if (std::holds_alternative<GraphPipeline::GraphIOStep>(step)) {
            const GraphPipeline::GraphIOStep& ioStep = std::get<GraphPipeline::GraphIOStep>(step);
            RETURN_IF_ERROR(ioStep.ioNode->RunAsIO());
        }
        else if (std::holds_alternative<ManualDataStep>(step)) {
            return absl::UnimplementedError("TODO: Implement ManualDataStep in PipelineRunner::RunPipeline()");
        }
        else {
            LOG(FATAL) << "Invalid step";
        }
    }
    // Collect the output values from the graph output nodes.
    for (const auto& ioNode : pipeline_.graphOutputs) {
        const NodeId nodeId = ioNode->GetNodeId();
        auto encodedOutput = ioNode->GetEncodedOutput();
        if (encodedOutput.ok()) {
            result.outputs[nodeId] = encodedOutput.value();
        } else {
            result.outputs[nodeId] = std::nullopt;
        }
    }
    return result;
}

}  // namespace ujcore
