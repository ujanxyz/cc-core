#include "ujcore/pipeline/PipelineBuilder.h"

#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/GraphPipeline.h"
#include "ujcore/pipeline/PipelineFnNode.h"
#include "ujcore/pipeline/PipelineSlot.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using ::ujcore::plinfo::SlotInfo;
using ::ujcore::plstate::SlotState;

using NodeRunStep = GraphPipeline::NodeRunStep;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;

}  // namespace

PipelineBuilder::PipelineBuilder(const GraphState& graph, GraphPipeline& pipeline)
    : graph_(graph), pipeline_(pipeline) {
}

absl::Status PipelineBuilder::Rebuild() {
    pipeline_.nodeStages.clear();
    pipeline_.execSteps.clear();
    LOG(INFO) << "Rebuilding pipeline from graph state, #nodes: " << graph_.nodeInfos.size()
              << ", #edges: " << graph_.edgeInfos.size();

    for (const auto& [nodeId, node] : graph_.nodeInfos) {
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

    for (const auto& [edgeId, edge] : graph_.edgeInfos) {
        PipelineSlot* srcSlot = LookupPipelineSlot(edge.node0, edge.slot0);
        if (srcSlot == nullptr) {
            return absl::NotFoundError(absl::StrCat("Source slot not found for edge: ", edgeId.value));
        }
        PipelineSlot* dstSlot = LookupPipelineSlot(edge.node1, edge.slot1);
        if (dstSlot == nullptr) {
            return absl::NotFoundError(absl::StrCat("Target slot not found for edge: ", edgeId.value));
        }
        edgeStepsByNode[edge.node1].push_back(EdgePropagateStep {
            .srcAttr = &srcSlot->attribute,
            .dstAttr = &dstSlot->attribute,
        });
    }

    int numEdgeSteps = 0;
    int numFnStages = 0;
    int numInStages = 0;
    int numOutStages = 0;

    const size_t numPipelineNodes = pipeline_.nodeStages.size();
    const size_t numSortedNodes = graph_.topoSortState.sortOrder.size();
    if (numSortedNodes != numPipelineNodes) {
        return absl::InternalError(
            absl::StrCat("Node count mismatch: ", numSortedNodes, " vs ", numPipelineNodes));
    }

    std::vector<GraphPipeline::ExecutionStep> executionSteps;
    executionSteps.reserve(pipeline_.nodeStages.size() + graph_.edgeInfos.size());

    // Append the node steps in topological order, preceeded by their incoming edge steps.
    for (const NodeId nodeId : graph_.topoSortState.sortOrder) {
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

        RETURN_IF_NOT_FOUND_IN_MAP(GraphPipeline::NodeStage& nodeStage, pipeline_.nodeStages, nodeId);
        if (nodeStage.ntype == plinfo::NodeInfo::NodeType::FN) {
            std::unique_ptr<PipelineFnNode>& fnNode = std::get<std::unique_ptr<PipelineFnNode>>(nodeStage.node);
            executionSteps.push_back(NodeRunStep {
                .fnNode = fnNode.get(),
            });
            ++numFnStages;
        } else {
            std::unique_ptr<PipelineIONode>& ioNode = std::get<std::unique_ptr<PipelineIONode>>(nodeStage.node);
            executionSteps.push_back(GraphPipeline::GraphIOStep {
                .ioNode = ioNode.get(),
            });
            if (nodeStage.ntype == plinfo::NodeInfo::NodeType::IN) {
                ++numInStages;
            } else {
                ++numOutStages;
            }
        }
    }

    if (!edgeStepsByNode.empty()) {
        return absl::InternalError("Unprocessed node found");
    }
    pipeline_.execSteps.swap(executionSteps);
    LOG(INFO) << "Built pipeline, #edges: " << numEdgeSteps << ", #fn nodes: " << numFnStages
              << ", #graph inputs: " << numInStages << ", #graph outputs: " << numOutStages;
    return absl::OkStatus();
}

absl::Status PipelineBuilder::HandleFunctionNode(const NodeId nodeId, const plinfo::NodeInfo& nodeInfo) {
    auto& registry = FunctionRegistry::GetInstance();
    const AttributeTypeRegistry& attrRegistry = AttributeTypeRegistry::GetInstance();

    std::unique_ptr<FunctionSpec> fnSpec = registry.GetSpecFromUri(nodeInfo.uri);
    std::unique_ptr<FunctionBase> fnInstance = registry.CreateFromUri(nodeInfo.uri);
    if (fnSpec == nullptr || fnInstance == nullptr) {
        LOG(FATAL) << "Failed to create function instance for uri: " << nodeInfo.uri;
    }
    std::unique_ptr<PipelineFnNode> fnNode = std::make_unique<PipelineFnNode>(nodeId, *fnSpec, std::move(fnInstance));

    for (const FuncParamSpec& param : fnSpec->params) {
        const SlotId slotId = {nodeId, param.name};
        auto slotInfoIter = graph_.slotInfos.find(slotId);
        auto slotStateIter = graph_.slotStates.find(slotId);
        if (slotInfoIter == graph_.slotInfos.end() || slotStateIter == graph_.slotStates.end()) {
            return absl::NotFoundError(absl::StrCat("Slot info / state not found: ", param.name));
        }
        const SlotInfo& slotInfo = slotInfoIter->second;
        const SlotState& slotState = slotStateIter->second;

        if (slotInfo.access == SlotInfo::AccessEnum::I || slotInfo.access == SlotInfo::AccessEnum::M) {
            if (!slotState.encodedData.has_value() && slotState.inEdges.empty()) {
                return absl::InvalidArgumentError(
                    absl::StrCat("Slot has no data source: ", nodeId.value, " : ", param.name));
            }
        }

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

        const AttributeDecodeFn* decodeFnPtr = nullptr;
        if (slotInfo.access == SlotInfo::AccessEnum::I || slotInfo.access == SlotInfo::AccessEnum::M) {
          decodeFnPtr = attrRegistry.GetDecodeFn(slotInfo.dtype);
            if (decodeFnPtr == nullptr) {
                return absl::InvalidArgumentError(
                    absl::StrCat("No decoder function found for graph input data type: ", slotInfo.dtype));
            }
        }
        
        fnNode->slotEntries_[param.name] = PipelineFnNode::PerSlotEntry {
            .dtype = AttributeDataTypeFromStr(slotInfo.dtype),
            .encodedInput = &slotState.encodedData,
            .decodeFnPtr = decodeFnPtr,
            .plSlot = std::make_unique<PipelineSlot>(std::move(plSlot)),
        };
    }
    pipeline_.nodeStages[nodeId] = GraphPipeline::NodeStage {
        .ntype = nodeInfo.ntype,
        .node = std::move(fnNode),
    };
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

    RETURN_IF_NOT_FOUND_IN_MAP(const plstate::SlotState& slotState, graph_.slotStates, slotId);
    RETURN_IF_NOT_FOUND_IN_MAP(const plstate::NodeState& nodeState, graph_.nodeStates, nodeId);

    if (!isOutput) {
        // It's a graph input. Its slot should not have incoming edges.
        if (!slotState.inEdges.empty()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph input node slot should not have incoming edges"));
        }        
        if (!nodeState.encodedData.has_value()) {
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
        if (nodeState.encodedData.has_value()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node should not have input data"));
        }
        if (slotState.inEdges.size() != 1) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node slot should have exactly one incoming edge"));
        }
    }

    const AttributeTypeRegistry& attrRegistry = AttributeTypeRegistry::GetInstance();

    std::unique_ptr<PipelineIONode> ioNode = std::make_unique<PipelineIONode>(nodeId, isOutput);
    ioNode->dtype_ = dtype;
    if (!isOutput) {
        ioNode->encodedInput_ = &nodeState.encodedData;
    }
    ioNode->slot_ = PipelineSlot {
        .access = isOutput ? FuncParamAccess::kInput : FuncParamAccess::kOutput,
        // IO node slots cannot have manual local data, they are connected to the external environment.
        .overridden = false,
    };

    if (isOutput) {
        const AttributeEncodeFn* encodeFn = attrRegistry.GetEncodeFn(slotInfo.dtype);
        if (encodeFn == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("No encoder function found for graph output data type: ", slotInfo.dtype));
        }
        ioNode->convertFnPtr_ = encodeFn;
    } else {
        const AttributeDecodeFn* decodeFn = attrRegistry.GetDecodeFn(slotInfo.dtype);
        if (decodeFn == nullptr) {
            return absl::InvalidArgumentError(
                absl::StrCat("No decoder function found for graph input data type: ", slotInfo.dtype));
        }
        ioNode->convertFnPtr_ = decodeFn;
    }
    pipeline_.nodeStages[nodeId] = GraphPipeline::NodeStage {
        .ntype = nodeInfo.ntype,
        .node = std::move(ioNode),
    };
    return absl::OkStatus();
}

PipelineSlot* PipelineBuilder::LookupPipelineSlot(NodeId nodeId, const std::string& slotName) {
    auto stageIter = pipeline_.nodeStages.find(nodeId);
    if (stageIter == pipeline_.nodeStages.end()) {
        return nullptr;
    }
    GraphPipeline::NodeStage& nodeStage = stageIter->second;
    if (nodeStage.ntype == plinfo::NodeInfo::NodeType::FN) {
        const std::unique_ptr<PipelineFnNode>& fnNode = std::get<std::unique_ptr<PipelineFnNode>>(nodeStage.node);
        return fnNode->LookupSlot(slotName);
    } else if (nodeStage.ntype == plinfo::NodeInfo::NodeType::IN || nodeStage.ntype == plinfo::NodeInfo::NodeType::OUT) {
        const std::unique_ptr<PipelineIONode>& ioNode = std::get<std::unique_ptr<PipelineIONode>>(nodeStage.node);
        return &ioNode->slot_;
    }
    return nullptr;
}

}  // namespace ujcore