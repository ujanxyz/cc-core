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
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/functions/attributes/BitmapAttr.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/FuncExecutor.h"
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
using NodeOpsStage = GraphPipeline::NodeOpsStage;

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

    const AttributeDataType dtype = AttributeDataTypeFromStr(slotInfo.dtype);
    if (dtype == AttributeDataType::kUnknown) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unsupported data type for graph IO node: ", slotInfo.dtype));
    }

    RETURN_IF_NOT_FOUND_IN_MAP(const grph::SlotState& slotState, graph_.slotStates, slotId);

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

    const AttributeTypeRegistry& attrRegistry = AttributeTypeRegistry::GetInstance();

    std::unique_ptr<PipelineIONode> ioNode = std::make_unique<PipelineIONode>(nodeId, isOutput);
    ioNode->dtype_ = dtype;
    if (!isOutput) {
            ioNode->encodedInput_ = &slotState.encodedData;
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

/*
As part of the re-implementation of the pipeline builder, are interested in only the field `nodeOpsStages` in
`GraphPipeline`, which uniformly handles manual data both in function nodes and graph IO nodes.

[*] Assume we have already registered encode and decode function of all data types, with the uri format
`/$IN/<T>` for a graph input of datatype T, and `/$OUT/<T>` for a graph output of datatype T.

[*] Now encoding manual data at input node and decoding at an output node is now also handled by
registered functions (with above special uris). But a function operates only on inputs and outputs (also inout a special type, not important here).
Therefore we have created a special datatype, named `$encoded` (which is encoded string) that serves as
the input in decode functions and output in encode functions. The actual encode/decode logic is
implemented in the registered functions.

[*] We also create special artificial slot entries (only in the pipeline, not mutating graph state)
to carry the manually overridden data of type `$encoded` for the encode/decode functions.
The slot name is "$encoded:<slotname>", derived from the original param name to/from which it provides or consumes data.

[*] Similar artificial slot entry is also used in those user function stages where some input
 param has manual data as opposed to edge-delivered data. This is to unify the data access in
 function execution.

INPUT NODE:
- If it is an input node, it will always have one encode function, and no user function.
- The node has a slot named "$out", which is an output slot. Suppose it datatype (`dtype`) is T.
- The fn-uri in the node should be of the form `/$IN/<T>`. 
- The `NodeOpsStage` stage should have only that encode function, and no edge transfer step.
- We create a special slot to serve as the input of the encode function, with name
  "$encoded:$out", and datatype `$encoded` (`kEncoded`).
- The function will be based on:
  uri: `/$IN/<T>` (Get it from `nodeInfo.fnuri`)
  input: dtype = `$encoded`, name: "$encoded:$in"
  output: dtype = T, name = "$out"
- During the build phase itself parse the encoded / manual data at the slot state and create the
  artificial attribute of type `$encoded`, and use it in the encode function.

OUTPUT NODE:
- If it is an output node, it will always have one decode function, and no user function.
- The node has a slot named "$in", which is an input slot. Suppose it datatype (`dtype`) is T.
- The fn-uri in the node should be of the form `/$OUT/<T>`. 
- The `NodeOpsStage` stage should have only that decode function, and no edge transfer step.
- We create a special slot to serve as the output of the decode function, with name
"$encoded:$in", and datatype `$encoded` (`kEncoded`)
- The function will be based on:
  uri: `/$OUT/<T>` (Get it from `nodeInfo.fnuri`)
  input: dtype = T, name = "$in"
  output: dtype = `$encoded`, name: "$encoded:$out"
- During the build phase create the artificial slot entry for the decode function,
  and after completion of the output node stage, extract the encoded data as graph output.

FUNCTION NODE:
- If it is a function node, it will have one user function, andone or more encode functions
 depending on whether it has manually overridden data in its input slots.
- For each of its input slot of name say `p1` and type T, whose state has encoded data, we
  create a special slot with name "$encoded:p1" and dtype `$encoded` to hold the encoded data,
  and add one encode function stage to convert it to internal attribute for the user function.

Rule for populating the `NodeOpsStage` for a node:
- `executors` has the list of alll functions to be executed in this node, in the order
they should be executed. This includes the encode function for input nodes, decode function
for output nodes, and user function for function nodes. For example, if a function node has
2 input params with manual data, then the `executors` list will have 3 functions:
[encode for p1, encode for p2, user function]. The user function will always be the last.
- Build stage need to know the internal of an atribute of type "$encoded" to be able to
  populate it with the manually overridden data.  This would be the type `EncodedAttr::Storage`
  defined in `ujcore/function/EncodedAttr.h`.
*/

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
            .edgeId = edgeId,
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

absl::Status PipelineBuilder::RebuildV2(const GraphState& graph, GraphPipeline& pipeline) {
    pipeline.nodeStages.clear();
    pipeline.nodeGroups.clear();
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
                const SlotId slotId = SlotId { .parent = nodeId, .name = "$out" };
                RETURN_IF_NOT_FOUND_IN_MAP(const grph::SlotState& slotState, graph.slotStates, slotId);
                std::optional<std::string> assetUriOpt;
                if (slotState.encodedData.has_value()) {
                    const std::optional<grph::EncodedData>& encodedDataOpt = slotState.encodedData;
                    ASSIGN_OR_RETURN(const std::string assetUri, GetBitmapAssetUriFromEncoded(encodedDataOpt->payload));
                    assetUriOpt = assetUri;
                } else {
                    return absl::InvalidArgumentError(
                        absl::StrCat("Graph input node (bitmap) should have encoded input data"));
                }
                // A graph input node only has one output slot.
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