#include "ujcore/pipeline/PipelineRunner.h"

#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/pipeline/PipelineBuilder.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using NodeRunStep = GraphPipeline::NodeRunStep;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using GraphIOStep = GraphPipeline::GraphIOStep;
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
            VLOG(1) << "Running EdgePropagateStep";
            const auto& edgeStep = std::get<EdgePropagateStep>(step);
            edgeStep.dstAttr->dtype = edgeStep.srcAttr->dtype;
            edgeStep.dstAttr->data = edgeStep.srcAttr->data;
            if (edgeStep.dstAttr->data == nullptr) {
                LOG(WARNING) << "Propagating null data for dtype: " << AttributeDataTypeToStr(edgeStep.dstAttr->dtype);
            }
        }
        else if (std::holds_alternative<NodeRunStep>(step)) {
            VLOG(1) << "Running NodeRunStep";
            const NodeRunStep& nodeStep = std::get<NodeRunStep>(step);
            ASSIGN_OR_RETURN(bool runStatus, nodeStep.fnNode->RunFunction());
            if (!runStatus) {
                return absl::InternalError("Node run failed");
            }
        }
        else if (std::holds_alternative<GraphIOStep>(step)) {
            VLOG(1) << "Running GraphIOStep";
            const GraphIOStep& ioStep = std::get<GraphIOStep>(step);
            PipelineIONode& ioNode = *ioStep.ioNode;
            RETURN_IF_ERROR(ioNode.RunAsIO());
            if (ioNode.isOutputStage()) {
                // Collect the output values from the graph output nodes.
                const NodeId nodeId = ioNode.GetNodeId();
                auto encodedOutput = ioNode.GetEncodedOutput();
                if (encodedOutput.ok()) {
                    result.outputs[nodeId] = encodedOutput.value();
                } else {
                    result.outputs[nodeId] = std::nullopt;
                }
            }
        }
        else if (std::holds_alternative<ManualDataStep>(step)) {
            VLOG(1) << "Running ManualDataStep";
            return absl::UnimplementedError("TODO: Implement ManualDataStep in PipelineRunner::RunPipeline()");
        }
        else {
            LOG(FATAL) << "Invalid step";
        }
    }
    return result;
}

}  // namespace ujcore
