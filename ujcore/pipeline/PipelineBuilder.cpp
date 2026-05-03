#include "ujcore/pipeline/PipelineBuilder.h"

#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
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

using GraphIOStep = GraphPipeline::GraphIOStep;
using NodeRunStep = GraphPipeline::NodeRunStep;
using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using NodeStage = GraphPipeline::NodeStage;

absl::StatusOr<NodeStage> InternalHandleFunctionNode(const GraphState& graph_, const NodeId nodeId, const plinfo::NodeInfo& nodeInfo) {
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
        const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
        if (dtype == AttributeDataType::kUnknown) {
            return absl::InvalidArgumentError(
                absl::StrCat("Unsupported data type for slot: ", slotInfo.dtype));
        }

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
    return NodeStage {
        .ntype = nodeInfo.ntype,
        .node = std::move(fnNode),
    };
    // NodeStage nodeStage = NodeStage {
    //     .ntype = nodeInfo.ntype,
    //     .node = std::move(fnNode),
    // };
    // pipeline_.nodeStages[nodeId] = std::move(nodeStage);
    // return absl::OkStatus();
}

absl::StatusOr<NodeStage> InternalHandleGraphIONode(const GraphState& graph_, const NodeId nodeId, const plinfo::NodeInfo& nodeInfo) {
    // if (!nodeInfo.ioDtype.has_value()) {
    //     return absl::InvalidArgumentError("Graph IO node must have ioDtype");
    // }
    // const AttributeDataType dtype = AttributeDataTypeFromStr(nodeInfo.ioDtype.value());
    // if (dtype == AttributeDataType::kUnknown) {
    //     return absl::InvalidArgumentError(
    //         absl::StrCat("Unsupported data type for graph IO node: ", nodeInfo.ioDtype.value()));
    // }

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

    // TODO: Use nodeinfo's ioDtype directly after we ensure the graph state is correctly populated with it, instead of looking up from slot info.
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

    return NodeStage {
        .ntype = nodeInfo.ntype,
        .node = std::move(ioNode),
    };
}

// Helper methods to get the pipeline slot reference.
PipelineSlot* LookupPipelineSlot(GraphPipeline& pipeline_, NodeId nodeId, const std::string& slotName) {
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

}  // namespace

//------------------------------ PipelineBuilder Implementation ----------------------------------

absl::Status PipelineBuilder::Rebuild(const GraphState& graph, GraphPipeline& pipeline) {
    pipeline.nodeStages.clear();
    pipeline.execSteps.clear();
    LOG(INFO) << "Rebuilding pipeline from graph state, #nodes: " << graph.nodeInfos.size()
              << ", #edges: " << graph.edgeInfos.size();

    for (const auto& [nodeId, node] : graph.nodeInfos) {
        switch (node.ntype) {
            case plinfo::NodeInfo::NodeType::FN: {
                ASSIGN_OR_RETURN(pipeline.nodeStages[nodeId], InternalHandleFunctionNode(graph, nodeId, node));
                break;
            }
            case plinfo::NodeInfo::NodeType::IN:
            case plinfo::NodeInfo::NodeType::OUT: {
                ASSIGN_OR_RETURN(pipeline.nodeStages[nodeId], InternalHandleGraphIONode(graph, nodeId, node));
                break;
            }
            default:
                return absl::InvalidArgumentError(
                    absl::StrCat("Unsupported node type: ", static_cast<int>(node.ntype)));
        }
    }
    // For each node id, collect the edge steps preceding it.
    std::map<NodeId, std::vector<EdgePropagateStep>> edgeStepsByNode;

    // Build the edge propagation steps, stage them grouped by their target node id.
    for (const auto& [edgeId, edge] : graph.edgeInfos) {
        PipelineSlot* srcSlot = LookupPipelineSlot(pipeline, edge.node0, edge.slot0);
        if (srcSlot == nullptr) {
            return absl::NotFoundError(absl::StrCat("Source slot not found for edge: ", edgeId.value));
        }
        PipelineSlot* dstSlot = LookupPipelineSlot(pipeline, edge.node1, edge.slot1);
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

    const size_t numPipelineNodes = pipeline.nodeStages.size();
    const size_t numSortedNodes = graph.topoSortState.sortOrder.size();
    if (numSortedNodes != numPipelineNodes) {
        return absl::InternalError(
            absl::StrCat("Node count mismatch: ", numSortedNodes, " vs ", numPipelineNodes));
    }

    std::vector<GraphPipeline::ExecutionStep> executionSteps;
    executionSteps.reserve(pipeline.nodeStages.size() + graph.edgeInfos.size());

    // Append the node steps in topological order, preceeded by their incoming edge steps.
    for (const NodeId nodeId : graph.topoSortState.sortOrder) {
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

        RETURN_IF_NOT_FOUND_IN_MAP(GraphPipeline::NodeStage& nodeStage, pipeline.nodeStages, nodeId);
        if (nodeStage.ntype == plinfo::NodeInfo::NodeType::FN) {
            std::unique_ptr<PipelineFnNode>& fnNode = std::get<std::unique_ptr<PipelineFnNode>>(nodeStage.node);
            executionSteps.push_back(NodeRunStep {
                .fnNode = fnNode.get(),
            });
            ++numFnStages;
        } else {
            std::unique_ptr<PipelineIONode>& ioNode = std::get<std::unique_ptr<PipelineIONode>>(nodeStage.node);
            executionSteps.push_back(GraphIOStep {
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

    pipeline.execSteps.swap(executionSteps);
    LOG(INFO) << "Built pipeline, #edges: " << numEdgeSteps << ", #fn nodes: " << numFnStages
              << ", #graph inputs: " << numInStages << ", #graph outputs: " << numOutStages;
    return absl::OkStatus();
}

absl::StatusOr<std::vector<AssetInfo>> PipelineBuilder::GetAssetInfos(const GraphState& graph, const GraphPipeline& pipeline) {
    std::vector<AssetInfo> assetInfos;
    // Iterate over the steps and process based on step type.
    for (const auto& step : pipeline.execSteps) {
        if (std::holds_alternative<NodeRunStep>(step)) {
            const NodeRunStep& nodeStep = std::get<NodeRunStep>(step);
            const PipelineFnNode* const fnNode = nodeStep.fnNode;
            if (fnNode == nullptr) {
                continue;
            }
            const NodeId nodeId = fnNode->GetFunctionNodeId();
            // For function nodes, we look at:
            // (1) The input slots which have manually entered encoded data.
            // (2) The output slots which create new assets.
            for (const auto& [slotName, slotEntry] : nodeStep.fnNode->slotEntries_) {
                // const PipelineSlot* plSlot = slotEntry.plSlot.get();
                // We only support bitmaps as asset types.
                if (slotEntry.dtype != AttributeDataType::kBitmap) {
                    continue;
                }

                const SlotId slotId = SlotId { .parent = nodeId, .name = slotName };
                RETURN_IF_NOT_FOUND_IN_MAP(const plinfo::SlotInfo& slotInfo, graph.slotInfos, slotId);
                if (slotInfo.access == SlotInfo::AccessEnum::I) {
                    // It is a function input slot of type asset.
                    if (slotEntry.encodedInput != nullptr && slotEntry.encodedInput->has_value()) {
                        // This input slot actually has an encoded data at the time of execution.
                        assetInfos.push_back(AssetInfo {
                            .slotId = slotId,
                            .assetType = AssetInfo::AssetType::MANUAL,
                            .dtype = AttributeDataTypeToStr(slotEntry.dtype),
                        });
                    }
                }
                else if (slotInfo.access == SlotInfo::AccessEnum::O) {
                    // It is a function output slot of type asset, i.e. it will create the asset.
                    assetInfos.push_back(AssetInfo {
                        .slotId = slotId,
                        .assetType = AssetInfo::AssetType::ARTIFACT,
                        .dtype = AttributeDataTypeToStr(slotEntry.dtype),
                    });
                }
                else if (slotInfo.access == SlotInfo::AccessEnum::M) {
                    // Not yet implemented.
                    return absl::UnimplementedError("Asset slots with read-write access are not yet supported");
                }
            } // end for slotEntries
        } // if NodeRunStep
        else if (std::holds_alternative<GraphIOStep>(step)) {
            const GraphIOStep& ioStep = std::get<GraphIOStep>(step);
            const PipelineIONode* const ioNode = ioStep.ioNode;
            if (ioNode == nullptr) {
                continue;
            }
            if (ioNode->dtype_ != AttributeDataType::kBitmap) {
                // Only process asset IO nodes.
                continue;
            }
            const NodeId nodeId = ioNode->GetNodeId();
            if (ioNode->isOutputStage()) {
                // A graph output node only has one input slot.
                const SlotId slotId = SlotId { .parent = nodeId, .name = "$in" };
                assetInfos.push_back(AssetInfo {
                    .slotId = slotId,
                    .assetType = AssetInfo::AssetType::GRAPHOUT,
                    .dtype = AttributeDataTypeToStr(ioNode->dtype_),
                });
            } else {
                // A graph input node only has one output slot.
                const SlotId slotId = SlotId { .parent = nodeId, .name = "$out" };
                assetInfos.push_back(AssetInfo {
                    .slotId = slotId,
                    .assetType = AssetInfo::AssetType::GRAPHIN,
                    .dtype = AttributeDataTypeToStr(ioNode->dtype_),
                });
            }
        } // if GraphIOStep
    }

    return assetInfos;
}

}  // namespace ujcore