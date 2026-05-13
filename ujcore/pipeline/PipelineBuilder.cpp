#include "ujcore/pipeline/PipelineBuilder.h"

#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/functions/attributes/BitmapAttr.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/GraphPipeline.h"
#include "ujcore/pipeline/PipelineFnNode.h"
#include "ujcore/pipeline/PipelineSlot.h"
#include "ujcore/utils/status_macros.h"

#include <functional>
#include <string>

namespace ujcore {
namespace {

using ::ujcore::grph::SlotInfo;
using ::ujcore::grph::SlotState;

using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using NodeStage = GraphPipeline::NodeStage;

absl::StatusOr<NodeStage> InternalHandleFunctionNode(const GraphState& graph_, const NodeId nodeId, const grph::NodeInfo& nodeInfo) {
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

        // TODO: Create a SlotStorage from `grph::SlotInfo` and add to the node.
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
}

absl::StatusOr<NodeStage> InternalHandleGraphIONode(const GraphState& graph_, const NodeId nodeId, const grph::NodeInfo& nodeInfo) {
    const auto slotInfos = GraphUtils::CopyAllSlotInfos(graph_, nodeId);
    if (slotInfos.size() != 1) {
        return absl::InvalidArgumentError(
            absl::StrCat("Graph IO node should have exactly one slot, found: ", slotInfos.size()));
    }
    const grph::SlotInfo& slotInfo = slotInfos[0];
    if ((nodeInfo.ntype == grph::NodeInfo::NodeType::IN &&
         slotInfo.access != grph::SlotInfo::AccessEnum::O) ||
        (nodeInfo.ntype == grph::NodeInfo::NodeType::OUT &&
         slotInfo.access != grph::SlotInfo::AccessEnum::I)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Graph input node should have output slot, and graph output node should have input slot"));
    }

    const bool isOutput = (nodeInfo.ntype == grph::NodeInfo::NodeType::OUT);
    const SlotId slotId = {.parent = nodeId, .name = slotInfo.name};

    // TODO: Use nodeinfo's ioDtype directly after we ensure the graph state is correctly populated with it, instead of looking up from slot info.
    const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
    if (dtype == AttributeDataType::kUnknown) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unsupported data type for graph IO node: ", slotInfo.dtype));
    }

    RETURN_IF_NOT_FOUND_IN_MAP(const grph::SlotState& slotState, graph_.slotStates, slotId);
    RETURN_IF_NOT_FOUND_IN_MAP(const grph::NodeState& nodeState, graph_.nodeStates, nodeId);

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
    if (nodeStage.ntype == grph::NodeInfo::NodeType::FN) {
        const std::unique_ptr<PipelineFnNode>& fnNode = std::get<std::unique_ptr<PipelineFnNode>>(nodeStage.node);
        return fnNode->LookupSlot(slotName);
    } else if (nodeStage.ntype == grph::NodeInfo::NodeType::IN || nodeStage.ntype == grph::NodeInfo::NodeType::OUT) {
        const std::unique_ptr<PipelineIONode>& ioNode = std::get<std::unique_ptr<PipelineIONode>>(nodeStage.node);
        return &ioNode->slot_;
    }
    return nullptr;
}

}  // namespace

//------------------------------ PipelineBuilder Implementation ----------------------------------

absl::Status PipelineBuilder::Rebuild(const GraphState& graph, GraphPipeline& pipeline) {
    pipeline.nodeStages.clear();
    pipeline.nodeGroups.clear();
    LOG(INFO) << "Rebuilding pipeline from graph state, #nodes: " << graph.nodeInfos.size()
              << ", #edges: " << graph.edgeInfos.size();

    for (const auto& [nodeId, node] : graph.nodeInfos) {
        switch (node.ntype) {
            case grph::NodeInfo::NodeType::FN: {
                ASSIGN_OR_RETURN(pipeline.nodeStages[nodeId], InternalHandleFunctionNode(graph, nodeId, node));
                break;
            }
            case grph::NodeInfo::NodeType::IN:
            case grph::NodeInfo::NodeType::OUT: {
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

    // Build node execution groups in topological order. Each group contains incoming edge
    // transfers and the node execution step, organized for efficient topo-order execution.
    for (const NodeId nodeId : graph.topoSortState.sortOrder) {
        RETURN_IF_NOT_FOUND_IN_MAP(GraphPipeline::NodeStage& nodeStage, pipeline.nodeStages, nodeId);
        
        // Extract incoming edge steps for this node.
        std::vector<EdgePropagateStep> incomingTransfers;
        auto iter = edgeStepsByNode.find(nodeId);
        if (iter != edgeStepsByNode.end()) {
            incomingTransfers = std::move(iter->second);
            edgeStepsByNode.erase(iter);
            numEdgeSteps += incomingTransfers.size();
        }

        if (nodeStage.ntype == grph::NodeInfo::NodeType::FN) {
            std::unique_ptr<PipelineFnNode>& fnNode = std::get<std::unique_ptr<PipelineFnNode>>(nodeStage.node);
            GraphPipeline::NodeExecGroup group {
                .nodeId = nodeId,
                .incomingTransfers = std::move(incomingTransfers),
                .nodeStep = std::ref(*fnNode),
            };
            pipeline.nodeGroups.push_back(std::move(group));
            ++numFnStages;
        } else {
            std::unique_ptr<PipelineIONode>& ioNode = std::get<std::unique_ptr<PipelineIONode>>(nodeStage.node);
            GraphPipeline::NodeExecGroup group {
                .nodeId = nodeId,
                .incomingTransfers = std::move(incomingTransfers),
                .nodeStep = std::ref(*ioNode),
            };
            pipeline.nodeGroups.push_back(std::move(group));
            if (nodeStage.ntype == grph::NodeInfo::NodeType::IN) {
                ++numInStages;
            } else {
                ++numOutStages;
            }
        }
    }

    if (!edgeStepsByNode.empty()) {
        return absl::InternalError("Unprocessed node found");
    }

    LOG(INFO) << "Built pipeline, #edges: " << numEdgeSteps << ", #fn nodes: " << numFnStages
              << ", #graph inputs: " << numInStages << ", #graph outputs: " << numOutStages;
    return absl::OkStatus();
}

absl::StatusOr<std::vector<AssetInfo>> PipelineBuilder::GetAssetInfos(const GraphState& graph, const GraphPipeline& pipeline) {
    std::vector<AssetInfo> assetInfos;
    // Iterate over the node execution groups and process based on node type.
    for (const auto& group : pipeline.nodeGroups) {
        const NodeId nodeId = group.nodeId;
        if (std::holds_alternative<std::reference_wrapper<PipelineFnNode>>(group.nodeStep)) {
            const PipelineFnNode& fnNode = std::get<std::reference_wrapper<PipelineFnNode>>(group.nodeStep).get();
            // For function nodes, we look at:
            // (1) The input slots which have manually entered encoded data.
            // (2) The output slots which create new assets.
            for (const auto& [slotName, slotEntry] : fnNode.slotEntries_) {
                // We only support bitmaps as asset types.
                if (slotEntry.dtype != AttributeDataType::kBitmap) {
                    continue;
                }

                const SlotId slotId = SlotId { .parent = nodeId, .name = slotName };
                RETURN_IF_NOT_FOUND_IN_MAP(const grph::SlotInfo& slotInfo, graph.slotInfos, slotId);
                if (slotInfo.access == SlotInfo::AccessEnum::I) {
                    // It is a function input slot of type asset.
                    if (slotEntry.encodedInput != nullptr && slotEntry.encodedInput->has_value()) {
                        // This input slot actually has an encoded data at the time of execution.
                        const std::optional<grph::EncodedData>& encodedDataOpt = *slotEntry.encodedInput;
                        ASSIGN_OR_RETURN(const std::string assetUri, GetBitmapAssetUriFromEncoded(encodedDataOpt->payload));
                        assetInfos.push_back(AssetInfo {
                            .slotId = slotId,
                            .assetType = AssetInfo::AssetType::MANUAL,
                            .dtype = AttributeDataTypeToStr(slotEntry.dtype),
                            .assetUri = assetUri,
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
        else if (std::holds_alternative<std::reference_wrapper<PipelineIONode>>(group.nodeStep)) {
            const PipelineIONode& ioNode = std::get<std::reference_wrapper<PipelineIONode>>(group.nodeStep).get();
            if (ioNode.dtype_ != AttributeDataType::kBitmap) {
                // Only process asset IO nodes.
                continue;
            }
            if (ioNode.isOutputStage()) {
                // A graph output node only has one input slot.
                const SlotId slotId = SlotId { .parent = nodeId, .name = "$in" };
                assetInfos.push_back(AssetInfo {
                    .slotId = slotId,
                    .assetType = AssetInfo::AssetType::GRAPHOUT,
                    .dtype = AttributeDataTypeToStr(ioNode.dtype_),
                });
            } else {
                RETURN_IF_NOT_FOUND_IN_MAP(const grph::NodeState& nodeState, graph.nodeStates, nodeId);
                std::optional<std::string> assetUriOpt;
                if (nodeState.encodedData.has_value()) {
                    const std::optional<grph::EncodedData>& encodedDataOpt = *nodeState.encodedData;
                    ASSIGN_OR_RETURN(const std::string assetUri, GetBitmapAssetUriFromEncoded(encodedDataOpt->payload));
                    assetUriOpt = assetUri;
                } else {
                    return absl::InvalidArgumentError(
                        absl::StrCat("Graph input node (bitmap) should have encoded input data"));
                }
                // A graph input node only has one output slot.                
                const SlotId slotId = SlotId { .parent = nodeId, .name = "$out" };
                assetInfos.push_back(AssetInfo {
                    .slotId = slotId,
                    .assetType = AssetInfo::AssetType::GRAPHIN,
                    .dtype = AttributeDataTypeToStr(ioNode.dtype_),
                    .assetUri = std::move(assetUriOpt),
                });
            }
        } // if GraphIOStep
    }

    return assetInfos;
}

}  // namespace ujcore