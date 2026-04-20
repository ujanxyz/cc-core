#include "ujcore/pipeline/PipelineBuilder.h"

#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/PipelineSlot.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using ::ujcore::plinfo::SlotInfo;
using ::ujcore::plstate::SlotState;

using NodeRunStep = GraphPipeline::NodeRunStep;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using ManualDataStep = GraphPipeline::ManualDataStep;

}  // namespace

PipelineBuilder::PipelineBuilder(const GraphState& graph, GraphPipeline& pipeline)
    : graph_(graph), pipeline_(pipeline) {
}

absl::Status PipelineBuilder::Rebuild() {
    pipeline_.nodes.clear();
    pipeline_.execSteps.clear();
    pipeline_.graphInputs.clear();
    pipeline_.graphOutputs.clear();

    for (const auto& [nodeId, node] : graph_.node_infos) {
        switch (node.ntype) {
            case plinfo::NodeInfo::NodeType::FN:
                RETURN_IF_ERROR(HandleFunctionNode(nodeId, node));
                break;
            case plinfo::NodeInfo::NodeType::IN:
            case plinfo::NodeInfo::NodeType::OUT:
                RETURN_IF_ERROR(HandleGraphIONode(nodeId, node));
                break;
            default:
                return absl::InvalidArgumentError(
                    absl::StrCat("Unsupported node type: ", static_cast<int>(node.ntype)));
        }
    }

    // For each node id, collect the edge steps preceding it.
    std::map<NodeId, std::vector<EdgePropagateStep>> edgeStepsByNode;

    for (const auto& [edgeId, edge] : graph_.edge_infos) {
        RETURN_IF_NOT_FOUND_IN_MAP(auto& n0, pipeline_.nodes, edge.node0);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& n1, pipeline_.nodes, edge.node1);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& slot0, n0->slots_, edge.slot0);
        RETURN_IF_NOT_FOUND_IN_MAP(auto& slot1, n1->slots_, edge.slot1);
        edgeStepsByNode[edge.node1].push_back(EdgePropagateStep {
            .srcAttr = &slot0.attribute,
            .dstAttr = &slot1.attribute,
        });
    }

    std::vector<GraphPipeline::ExecutionStep> executionSteps;
    executionSteps.reserve(pipeline_.nodes.size() + graph_.edge_infos.size());

    int numNodeSteps = 0;
    int numEdgeSteps = 0;

    const size_t numSortedNodes = graph_.topoSortState.sortOrder.size();
    if (numSortedNodes != pipeline_.nodes.size()) {
        return absl::InternalError(
            absl::StrCat("Node count mismatch: ", numSortedNodes, " vs ", pipeline_.nodes.size()));
    }

    // Start with the graph input stages.
    for (const auto& ioNode : pipeline_.graphInputs) {
        executionSteps.push_back(GraphPipeline::GraphIOStep {
            .ioNode = ioNode.get(),
        });
    }

    // Append the node steps in topological order, preceeded by their incoming edge steps.
    for (const NodeId nodeId : graph_.topoSortState.sortOrder) {
        RETURN_IF_NOT_FOUND_IN_MAP(auto& plNode, pipeline_.nodes, nodeId);

        // Extract (remove) the edge steps preceding this node.
        std::vector<EdgePropagateStep> edgeSteps;
        auto iter = edgeStepsByNode.find(nodeId);
        if (iter != edgeStepsByNode.end()) {
            edgeSteps = std::move(iter->second);
            edgeStepsByNode.erase(iter);
        }

        // Add the node step preceed by the edge steps (its incoming edges).
        for (auto& entry : edgeSteps) {
            executionSteps.push_back(std::move(entry));
            ++numEdgeSteps;
        }
        executionSteps.push_back(NodeRunStep {
            .fnNode = plNode.get(),
        });
        ++numNodeSteps;
    }

    // Finish with the graph output stages.
    for (const auto& ioNode : pipeline_.graphOutputs) {
        executionSteps.push_back(GraphPipeline::GraphIOStep {
            .ioNode = ioNode.get(),
        });
    }

    if (!edgeStepsByNode.empty()) {
        return absl::InternalError("Unprocessed node found");
    }
    pipeline_.execSteps.swap(executionSteps);
    LOG(INFO) << "Built pipeline, nodes: " << numNodeSteps << ", edges: " << numEdgeSteps;
    return absl::OkStatus();
}

absl::Status PipelineBuilder::HandleFunctionNode(const NodeId nodeId, const plinfo::NodeInfo& nodeInfo) {
    auto& registry = FunctionRegistry::GetInstance();

    std::unique_ptr<FunctionSpec> fnSpec = registry.GetSpecFromUri(nodeInfo.uri);
    std::unique_ptr<FunctionBase> fnInstance = registry.CreateFromUri(nodeInfo.uri);
    if (fnSpec == nullptr || fnInstance == nullptr) {
        LOG(FATAL) << "Failed to create function instance for uri: " << nodeInfo.uri;
    }
    std::unique_ptr<PipelineFnNode> fnNode = std::make_unique<PipelineFnNode>(nodeId, *fnSpec, std::move(fnInstance));

    for (const FuncParamSpec& param : fnSpec->params) {
        const SlotId slotId = {nodeId, param.name};
        auto slotInfoIter = graph_.slot_infos.find(slotId);
        auto slotStateIter = graph_.slot_states.find(slotId);
        if (slotInfoIter == graph_.slot_infos.end() || slotStateIter == graph_.slot_states.end()) {
            return absl::NotFoundError(absl::StrCat("Slot info / state not found: ", param.name));
        }
        const SlotInfo& slotInfo = slotInfoIter->second;
        const SlotState& slotState = slotStateIter->second;

        if (slotInfo.access == SlotInfo::AccessEnum::I || slotInfo.access == SlotInfo::AccessEnum::M) {
            if (!slotState.manual.has_value() && slotState.inEdges.empty()) {
                return absl::InvalidArgumentError(
                    absl::StrCat("Slot has no data source: ", nodeId.value, " : ", param.name));
            }
        }

        // TODO: Create `ManualDataStep` entry from `slotState.manual`

        // TODO: Create a SlotStorage from `plinfo::SlotInfo` and add to the node.
        FuncParamAccess access = FuncParamAccess::kUnknown;
        switch (slotInfo.access) {
            case SlotInfo::AccessEnum::I:
                access = FuncParamAccess::kInput;
                break;
            case SlotInfo::AccessEnum::O:
                access = FuncParamAccess::kOutput;
                break;
            case SlotInfo::AccessEnum::M:
                access = FuncParamAccess::kInOut;
                break;
        }

        PipelineSlot plSlot = {
            .access = access,
            .attribute = AttributeData {},
        };

        if (slotState.manual.has_value()) {
            //const EncodedData& manual = slotState.manual.value();
            //plSlot.overridden = true;
            //plSlot.attribute.dtype = AttributeDataTypeFromStr(slotInfo.dtype);
            // TODO: Populate plSlot.attribute.data
            return absl::UnimplementedError("TODO: not implemented");
        }

        fnNode->slots_[param.name] = std::move(plSlot);
    }
    pipeline_.nodes[nodeId] = std::move(fnNode);
    return absl::OkStatus();
}

absl::Status PipelineBuilder::HandleGraphIONode(const NodeId nodeId, const plinfo::NodeInfo& nodeInfo) {
    const auto slotInfos = GraphUtils::CopyAllSlotInfos(graph_, nodeId);
    if (slotInfos.size() != 1) {
        return absl::InvalidArgumentError(
            absl::StrCat("Graph IO node should have exactly one slot, found: ", slotInfos.size()));
    }
    const plinfo::SlotInfo& slotInfo = slotInfos[0];
    if ((nodeInfo.ntype == plinfo::NodeInfo::NodeType::IN &&
         slotInfo.access != plinfo::SlotInfo::AccessEnum::O) ||
        (nodeInfo.ntype == plinfo::NodeInfo::NodeType::OUT &&
         slotInfo.access != plinfo::SlotInfo::AccessEnum::I)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Graph input node should have output slot, and graph output node should have input slot"));
    }

    const bool isOutput = (nodeInfo.ntype == plinfo::NodeInfo::NodeType::OUT);
    const SlotId slotId = {.parent = nodeId, .name = slotInfo.name};

    const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
    if (dtype == AttributeDataType::kUnknown) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unsupported data type for graph IO node: ", slotInfo.dtype));
    }

    RETURN_IF_NOT_FOUND_IN_MAP(const plstate::SlotState& slotState, graph_.slot_states, slotId);
    RETURN_IF_NOT_FOUND_IN_MAP(const plstate::NodeState& nodeState, graph_.node_states, nodeId);

    if (!isOutput) {
        // It's a graph input. Its slot should not have incoming edges.
        if (!slotState.inEdges.empty()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph input node slot should not have incoming edges"));
        }        
        if (!nodeState.ioData.has_value()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph input node should have input data"));
        }
        if (slotState.outEdges.empty()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph input node slot should have outgoing edges"));
        }
    }
    if (isOutput) {
        // It's a graph output. Its slot should not have outgoing edges.
        if (!slotState.outEdges.empty()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node slot should not have outgoing edges"));
        }
        if (nodeState.ioData.has_value()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node should not have input data"));
        }
        if (slotState.inEdges.size() != 1) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node slot should have exactly one incoming edge"));
        }
    }

    const AttributeTypeRegistry& registry = AttributeTypeRegistry::GetInstance();

    std::unique_ptr<PipelineIONode> ioNode = std::make_unique<PipelineIONode>(nodeId, isOutput);
    ioNode->dtype_ = dtype;
    if (!isOutput) {
        ioNode->encodedInput_ = &nodeState.ioData;
    }
    ioNode->slot_ = PipelineSlot {
        .access = isOutput ? FuncParamAccess::kInput : FuncParamAccess::kOutput,
        // IO node slots cannot have manual local data, they are connected to the external environment.
        .overridden = false,
    };

    if (isOutput) {
        const AttributeEncodeFn* encodeFn = registry.GetEncodFn(slotInfo.dtype);
        if (encodeFn == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("No encoder function found for graph output data type: ", slotInfo.dtype));
        }
        ioNode->convertFnPtr_ = encodeFn;
        pipeline_.graphOutputs.push_back(std::move(ioNode));
    } else {
        const AttributeDecodeFn* decodeFn = registry.GetDecodFn(slotInfo.dtype);
        if (decodeFn == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("No decoder function found for graph input data type: ", slotInfo.dtype));
        }
        ioNode->convertFnPtr_ = decodeFn;
        pipeline_.graphInputs.push_back(std::move(ioNode));
    }
    return absl::OkStatus();
}

}  // namespace ujcore