#include "ujcore/pipeline/PipelineBuilder.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ujcore/function/EncodedAttr.h"
#include "ujcore/graph/AbslStringifies.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/FuncExecutor.h"
#include "ujcore/pipeline/GraphPipeline.h"
#include "ujcore/utils/status_macros.h"

#include <string>

namespace ujcore {
namespace {

using ::ujcore::grph::SlotInfo;
using ::ujcore::grph::SlotState;

using EdgePropagateStep = GraphPipeline::EdgePropagateStep;
using NodeOpsStage = GraphPipeline::NodeOpsStage;

using SlotInfoAndState = std::pair<const grph::SlotInfo*, const grph::SlotState*>;
using EncodedStorage = EncodedAttr::Storage;

//===================================
absl::Status _ProcessIoNode(const GraphState& graph, const grph::NodeInfo& nodeInfo, NodeOpsStage& nodeOps) {
    const grph::SlotInfo* pSlotInfo = nullptr;
    const grph::SlotState* pSlotState = nullptr;
    ASSIGN_OR_RETURN(std::tie(pSlotInfo, pSlotState), GraphUtils::LookupIoSlot(graph, nodeInfo.rawId));
    const grph::SlotInfo& slotInfo = *pSlotInfo;
    const grph::SlotState& slotState = *pSlotState;

    if ((nodeInfo.ntype == grph::NodeInfo::NodeType::IN &&
         slotInfo.access != grph::SlotInfo::AccessEnum::O) ||
        (nodeInfo.ntype == grph::NodeInfo::NodeType::OUT &&
         slotInfo.access != grph::SlotInfo::AccessEnum::I)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Graph input node should have output slot, and graph output node should have input slot"));
    }

    const bool isOutput = (nodeInfo.ntype == grph::NodeInfo::NodeType::OUT);
    const SlotId slotId = {.parent = nodeInfo.rawId, .name = slotInfo.name};

    const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
    if (dtype == AttributeDataType::kUnknown) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unsupported data type for graph IO node: ", slotInfo.dtype));
    }

    if (!isOutput) {
        // It's a graph input. Its slot should not have incoming edges.
        if (!slotState.inEdges.empty()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph input node slot should not have incoming edges"));
        }        
            if (!slotState.encodedData.has_value()) {
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
            if (slotState.encodedData.has_value()) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node should not have input data"));
        }
        if (slotState.inEdges.size() != 1) {
            return absl::InvalidArgumentError(
                absl::StrCat("Graph output node slot should have exactly one incoming edge"));
        }
    }

    auto& registry = FunctionRegistry::GetInstance();
    std::unique_ptr<FunctionBase> ioFuncInstance = registry.CreateFromUri(nodeInfo.uri);

    auto executor = std::make_unique<FuncExecutor>(nodeInfo.rawId, std::move(ioFuncInstance));
    executor->debugName_ = absl::StrCat("GraphIO-Fn - ", nodeInfo.uri);

    if (!isOutput) {
        // Graph input node.
        executor->type_ = FuncExecutor::ExecutorType::kDecode;
        executor->slotEntries_["$out"] = FuncExecutor::PerSlotEntry {
            .dtype = dtype,
            .access = FuncParamAccess::kOutput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kUnknown,
                .data = nullptr,
            },  
        };
        // Add a synthetic slot of type "$encoded" (kEncoded) with a pre-created attribute to hold
        // the encoded manual data.
        std::shared_ptr<EncodedStorage> storage = std::make_shared<EncodedStorage>();
        storage->encodedData = slotState.encodedData->payload;

        executor->slotEntries_["$in"] = FuncExecutor::PerSlotEntry {
            .dtype = AttributeDataType::kEncoded,
            .access = FuncParamAccess::kInput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kEncoded,
                .data = std::move(storage),
            },  
        };
    } else {
        executor->type_ = FuncExecutor::ExecutorType::kEncode;
        // Graph output node.
        executor->slotEntries_["$in"] = FuncExecutor::PerSlotEntry {
            .dtype = dtype,
            .access = FuncParamAccess::kInput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kUnknown,
                .data = nullptr,
            },  
        };

        executor->slotEntries_["$out"] = FuncExecutor::PerSlotEntry {
            .dtype = AttributeDataType::kEncoded,
            .access = FuncParamAccess::kOutput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kUnknown,
                .data = nullptr,
            },  
        };
    }
    // I/O nodes have only one executor.
    nodeOps.executors.push_back(std::move(executor));

    nodeOps.nodeId = nodeInfo.rawId;
    return absl::OkStatus();
}

absl::Status _ProcessFuncNode(const GraphState& graph_, const grph::NodeInfo& nodeInfo, NodeOpsStage& nodeOps) {
    std::vector<std::string> allInSlotNames;
    allInSlotNames.append_range(nodeInfo.ins);
    allInSlotNames.append_range(nodeInfo.outs);
    allInSlotNames.append_range(nodeInfo.inouts);

    // Instantiate the node's main function.
    auto& registry = FunctionRegistry::GetInstance();
    std::unique_ptr<FunctionBase> mainFuncInstance = registry.CreateFromUri(nodeInfo.uri);
    auto mainExecutor = std::make_unique<FuncExecutor>(nodeInfo.rawId, std::move(mainFuncInstance));
    mainExecutor->type_ = FuncExecutor::ExecutorType::kFunction;
    mainExecutor->debugName_ = absl::StrCat("MainFn - ", nodeInfo.uri);

    for (const std::string& slotName : allInSlotNames) {
        const SlotId slotId = {.parent = nodeInfo.rawId, .name = slotName};
        auto slotInfoIter = graph_.slotInfos.find(slotId);
        auto slotStateIter = graph_.slotStates.find(slotId);
        if (slotInfoIter == graph_.slotInfos.end() || slotStateIter == graph_.slotStates.end()) {
            return absl::NotFoundError(absl::StrCat("Slot info / state not found: ", slotName));
        }
        const SlotInfo& slotInfo = slotInfoIter->second;
        const SlotState& slotState = slotStateIter->second;

        const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
        if (dtype == AttributeDataType::kUnknown) {
            return absl::InvalidArgumentError(
                absl::StrCat("Unsupported data type for slot: ", slotInfo.dtype));
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

        FuncExecutor::PerSlotEntry& paramSlotEntry = mainExecutor->slotEntries_[slotName] = FuncExecutor::PerSlotEntry {
            .dtype = dtype,
            .access = access,
            // The actual attribute will be populated during execution.
            .attribute = AttributeData {
                .dtype = AttributeDataType::kUnknown,
                .data = nullptr,
            },
        };

        //----- handle encoded data -------

        if (!slotState.encodedData.has_value()) {
            // Output params (pure output, not in-out) cannot have manual override data.
            // Also inputs without override data do not need the following special handling.
            LOG(INFO) << "No manual data, uri:" << nodeInfo.uri << ", nodId:" << nodeInfo.rawId.value << ", slot: " << slotName;
            continue;
        }

        const std::string decodeFnUri = absl::StrCat("/$IN/", slotInfo.dtype);
        auto executor = std::make_unique<FuncExecutor>(nodeInfo.rawId, registry.CreateFromUri(decodeFnUri));
        executor->type_ = FuncExecutor::ExecutorType::kDecode;
        executor->debugName_ = absl::StrCat("DecodeFn for ", slotName, " - ", decodeFnUri);

        // Add a synthetic slot of type "$encoded" (kEncoded) with a pre-created attribute to hold
        // the encoded manual data.
        std::shared_ptr<EncodedStorage> storage = std::make_shared<EncodedStorage>();
        storage->encodedData = slotState.encodedData->payload;

        auto& execInSlot = executor->slotEntries_["$in"] = FuncExecutor::PerSlotEntry {
            .dtype = AttributeDataType::kEncoded,
            .access = FuncParamAccess::kInput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kEncoded,
                .data = std::move(storage),
            },
        };
        (void) execInSlot;

        auto& execOutSlot = executor->slotEntries_["$out"] = FuncExecutor::PerSlotEntry {
            .dtype = dtype,
            .access = FuncParamAccess::kOutput,
            .attribute = AttributeData {
                .dtype = AttributeDataType::kUnknown,
                .data = nullptr,
            },
        };

        EdgePropagateStep attrStep = {
            .edgeId = EdgeId(0),  // Dummy edge id since this is not from a real edge.
            .srcAttr = &execOutSlot.attribute,
            .dstAttr = &paramSlotEntry.attribute,
            .debugName = absl::StrCat("ManualDataTransfer for ", slotName),
        };
        nodeOps.attrTransfers.push_back(std::move(attrStep));

        nodeOps.executors.push_back(std::move(executor));
    }

    // User function is always the last to execute in the node.
    nodeOps.executors.push_back(std::move(mainExecutor));
    nodeOps.nodeId = nodeInfo.rawId;
    return absl::OkStatus();
}

// Helper methods to get the pipeline slot reference.
FuncExecutor::PerSlotEntry* LookupExecutorSlot(GraphPipeline& pipeline_, NodeId nodeId, const std::string& slotName) {
    auto stageIter = pipeline_.nodeOpsStages.find(nodeId);
    if (stageIter == pipeline_.nodeOpsStages.end()) {
        LOG(INFO) << "NodeOpsStage not found for nodeId: " << nodeId.value;
        return nullptr;
    }
    GraphPipeline::NodeOpsStage& nodeOpsStage = stageIter->second;
    const auto& lastExecutor = nodeOpsStage.executors.back();
    auto slotIter = lastExecutor->slotEntries_.find(slotName);
    if (slotIter == lastExecutor->slotEntries_.end()) {
        LOG(INFO) << "Slot entry not found in executor for slotName: " << slotName;
        return nullptr;
    }
    FuncExecutor::PerSlotEntry* slot = &slotIter->second;
    return slot;
}

}  // namespace

//------------------------------ PipelineBuilder Implementation ----------------------------------

absl::Status PipelineBuilder::RebuildV2(const GraphState& graph, GraphPipeline& pipeline) {
    pipeline.nodeOpsStages.clear();

    LOG(INFO) << "Rebuilding (V2) pipeline from graph state, #nodes: " << graph.nodeInfos.size()
              << ", #edges: " << graph.edgeInfos.size();

    for (const auto& [nodeId, node] : graph.nodeInfos) {
        switch (node.ntype) {
            case grph::NodeInfo::NodeType::FN: {
                GraphPipeline::NodeOpsStage nodeOps;
                RETURN_IF_ERROR(_ProcessFuncNode(graph, node, nodeOps));
                pipeline.nodeOpsStages[nodeId] = std::move(nodeOps);
                break;
            }
            case grph::NodeInfo::NodeType::IN:
            case grph::NodeInfo::NodeType::OUT: {
                GraphPipeline::NodeOpsStage nodeOps;
                RETURN_IF_ERROR(_ProcessIoNode(graph, node, nodeOps));
                pipeline.nodeOpsStages[nodeId] = std::move(nodeOps);
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
        // FuncExecutor::PerSlotEntry* LookupExecutorSlot(GraphPipeline& pipeline_, NodeId nodeId, const std::string& slotName)
        FuncExecutor::PerSlotEntry* srcSlot = LookupExecutorSlot(pipeline, edge.node0, edge.slot0);
        if (srcSlot == nullptr) {
            return absl::NotFoundError(absl::StrCat("Source slot not found for edge: ", edge));
        }
        FuncExecutor::PerSlotEntry* dstSlot = LookupExecutorSlot(pipeline, edge.node1, edge.slot1);
        if (dstSlot == nullptr) {
            return absl::NotFoundError(absl::StrCat("Target slot not found for edge: ", edge));
        }
        edgeStepsByNode[edge.node1].push_back(EdgePropagateStep {
            .edgeId = edgeId,
            .srcAttr = &srcSlot->attribute,
            .dstAttr = &dstSlot->attribute,
        });
    }

    int numEdgeSteps = 0;
    // int numFnStages = 0;
    // int numInStages = 0;
    // int numOutStages = 0;

    // Build node execution groups in topological order. Each group contains incoming edge
    // transfers and the node execution step, organized for efficient topo-order execution.
    for (const NodeId nodeId : graph.topoSortState.sortOrder) {
        RETURN_IF_NOT_FOUND_IN_MAP(GraphPipeline::NodeOpsStage& nodeStage, pipeline.nodeOpsStages, nodeId);
        
        // Extract incoming edge steps for this node.
        std::vector<EdgePropagateStep> incomingTransfers;
        auto iter = edgeStepsByNode.find(nodeId);
        if (iter != edgeStepsByNode.end()) {
            incomingTransfers = std::move(iter->second);
            edgeStepsByNode.erase(iter);
            numEdgeSteps += incomingTransfers.size();
        }

        nodeStage.attrTransfers.append_range(std::move(incomingTransfers));
        pipeline.stageSequence.push_back(&nodeStage);
    }


    {
        // Debug lines for the built pipeline.
        std::vector<std::string> debugLines;
        for (const auto& [nodeId, nodeStage] : pipeline.nodeOpsStages) {
            std::vector<std::string> transferStrs;
            for (const auto& transfer : nodeStage.attrTransfers) {
                std::string transferStr = absl::StrCat("(Edge ", transfer.edgeId.value, ": ", transfer.debugName, " - ", transfer.debugName, ")");
                transferStrs.push_back(std::move(transferStr));
            }
            std::vector<std::string> executorStrs;
            for (const auto& executor : nodeStage.executors) {
                executorStrs.push_back(executor->debugName_);
            }

            std::string str = absl::StrCat("NodeStage => ", nodeStage.nodeId.value, ", #executors", nodeStage.executors.size(), ", #attr-tranfers:", nodeStage.attrTransfers.size());
            str += "; transfers: " + absl::StrJoin(transferStrs, ", ");
            str += "; executors: " + absl::StrJoin(executorStrs, ", ");
            debugLines.push_back(std::move(str));
        }
        LOG(INFO) << "Built V2 pipeline with " << pipeline.nodeOpsStages.size() << " stages. Debug info:\n" << absl::StrJoin(debugLines, "\n");
    }
    return absl::OkStatus();
}

}  // namespace ujcore