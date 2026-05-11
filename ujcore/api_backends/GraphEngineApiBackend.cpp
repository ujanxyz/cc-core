#include "ujcore/api_schemas/GraphEngineApi.h"

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/GraphStateJson.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionLookup.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/graph/GraphBuilder.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/pipeline/PipelineRunner.h"
#include "ujcore/utils/status_macros.h"

namespace ujcore {
namespace {

using GetGraphResponse = GraphEngineApi::GetGraphResponse;
using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;
using CreateIONodeRequest = GraphEngineApi::CreateIONodeRequest;
using CreateIONodeResponse = GraphEngineApi::CreateIONodeResponse;
using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;
using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;
using ValidateEdgeRequest = GraphEngineApi::ValidateEdgeRequest;
using ValidateEdgeResponse = GraphEngineApi::ValidateEdgeResponse;
using GetNodeStatesRequest = GraphEngineApi::GetNodeStatesRequest;
using GetNodeStatesResponse = GraphEngineApi::GetNodeStatesResponse;
using GetSlotStatesRequest = GraphEngineApi::GetSlotStatesRequest;
using GetSlotStatesResponse = GraphEngineApi::GetSlotStatesResponse;
using GetAvailableFuncsRequest = GraphEngineApi::GetAvailableFuncsRequest;
using GetAvailableFuncsResponse = GraphEngineApi::GetAvailableFuncsResponse;
using SetEncodedDataRequest = GraphEngineApi::SetEncodedDataRequest;
using BuildPipelineResponse = GraphEngineApi::BuildPipelineResponse;
using RunPipelineResponse = GraphEngineApi::RunPipelineResponse;
using GetResourcesResponse = GraphEngineApi::GetResourcesResponse;

FunctionInfo ToFunctionInfo(const FunctionSpec& spec) {
    std::vector<FunctionInfo::Param> params;
    params.reserve(spec.params.size());
    for (const FuncParamSpec& p : spec.params) {
        FunctionInfo::AccessEnum access = FunctionInfo::AccessEnum::I;
        switch (p.access) {
            case FuncParamAccess::kInput: access = FunctionInfo::AccessEnum::I; break;
            case FuncParamAccess::kOutput: access = FunctionInfo::AccessEnum::O; break;
            case FuncParamAccess::kInOut:  access = FunctionInfo::AccessEnum::M; break;
            default:
                LOG(WARNING) << "Unknown FuncParamAccess value for param " << p.name
                    << ", defaulting to Input access.";
                access = FunctionInfo::AccessEnum::I;
        }
        params.push_back({
            .name = p.name,
            .type = AttributeDataTypeToStr(p.dtype),
            .access = access,
        });
    }
    return FunctionInfo{
        .uri = spec.uri,
        .label = spec.label,
        .desc = spec.desc,
        .params = std::move(params),
    };
}

bool HasFilter(
    const std::vector<GetAvailableFuncsRequest::CategoryFilter>& filters,
    const GetAvailableFuncsRequest::CategoryFilter wanted) {
    for (const auto filter : filters) {
        if (filter == wanted) {
            return true;
        }
    }
    return false;
}

}  // namespace

class GraphEngineApiBackend : public cppschema::ApiBackend<GraphEngineApi> {
 public:
    GraphEngineApiBackend(): topoSorter_(state_.topoSortState), builder_(state_, topoSorter_) {}

    GetGraphResponse getGraphImpl(const VoidType& kvoid) {
        return GetGraphResponse {
            .nodeInfos = GraphUtils::GetAllNodeInfos(state_),
            .edgeInfos = GraphUtils::GetAllEdgeInfos(state_),
            .slotInfos = GraphUtils::GetAllSlotInfos(state_),
        };
    }

    std::string encodeGraphImpl(const VoidType& kvoid) {
        auto encodedOr = EncodeGraphState(state_);
        if (!encodedOr.ok()) {
            LOG(FATAL) << "Encode graph state error: " << encodedOr.status();
        }
        return std::move(encodedOr).value();
    }

    VoidType decodeGraphImpl(const std::string& encoded) {
        auto decodedOr = DecodeGraphState(encoded);
        if (!decodedOr.ok()) {
            LOG(FATAL) << "Decode graph state error: " << decodedOr.status();
        }
        state_ = std::move(decodedOr).value();
        // TODO: Update topoSorter_ and builder_ with the new state. Currently they hold dangling references after state_ is replaced.
        // topoSorter_ = TopoSorter(state_.topoSortState);
        // builder_ = GraphBuilder(state_, topoSorter_);
        return VoidType{};
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        auto nodeInfoOr = builder_.AddFuncNode(request.func);
        if (!nodeInfoOr.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfoOr.status();
        }
        const NodeId nodeId = nodeInfoOr->rawId;
        const grph::NodeInfo nodeInfo = std::move(nodeInfoOr).value();

        auto addNodeSlotInfos = [this, nodeRawId = nodeId](const std::vector<std::string>& slotNames, std::vector<grph::SlotInfo>& result) -> void {
            auto slotsOr = builder_.LookupNodeSlotInfos(NodeId(nodeRawId), slotNames);
            if (!slotsOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotsOr.status();
            }
            result = std::move(slotsOr).value();
        };
        auto addNodeSlotStates = [this, nodeRawId = nodeId](const std::vector<std::string>& slotNames, std::vector<grph::SlotState>& result) -> void {
            std::vector<SlotId> slotIds;
            slotIds.reserve(slotNames.size());
            for (const std::string& slotName : slotNames) {
                slotIds.push_back(SlotId{nodeRawId, slotName});
            }
            auto slotStatesOr = GraphUtils::LookupSlotStates(state_, slotIds);
            if (!slotStatesOr.ok()) {
                LOG(FATAL) << "Lookup node slots error: " << slotStatesOr.status();
            }
            std::vector<grph::SlotState> states;
            states.reserve(slotStatesOr->size());
            for (auto& [_, slotState] : std::move(slotStatesOr).value()) {
                states.push_back(std::move(slotState));
            }
            result = std::move(states);
        };

        CreateNodeResponse response;
        addNodeSlotInfos(nodeInfo.ins, response.inInfos);
        addNodeSlotInfos(nodeInfo.outs, response.outInfos);
        addNodeSlotInfos(nodeInfo.inouts, response.inoutInfos);
        addNodeSlotStates(nodeInfo.ins, response.inStates);
        addNodeSlotStates(nodeInfo.outs, response.outStates);
        addNodeSlotStates(nodeInfo.inouts, response.inoutStates);
        response.nodeInfo = std::move(nodeInfo);
        response.nodeState = GraphUtils::CopyNodeState(state_, nodeId);
        return response;
    }

    CreateIONodeResponse createIONodeImpl(const CreateIONodeRequest& request) {
        auto nodeSlotInfoOr = builder_.AddIONode(request.dtype, request.isOutput);
        if (!nodeSlotInfoOr.ok()) {
            LOG(FATAL) << "Insert IO node error: " << nodeSlotInfoOr.status();
        }
        grph::NodeInfo nodeInfo;
        grph::SlotInfo slotInfo;
        std::tie(nodeInfo, slotInfo) = std::move(nodeSlotInfoOr).value();

        const NodeId nodeId = nodeInfo.rawId;
        auto slotStateOr = GraphUtils::LookupSlotStates(state_, { SlotId{nodeId, slotInfo.name} });
        if (!slotStateOr.ok() || slotStateOr->size() != 1) {
            LOG(FATAL) << "Lookup IO node slot state error: " << slotStateOr.status();
        }
        const grph::SlotState slotState = std::move(slotStateOr).value().begin()->second;
        return CreateIONodeResponse {
            .nodeInfo = std::move(nodeInfo),
            .nodeState = GraphUtils::CopyNodeState(state_, nodeId),
            .slotInfo = std::move(slotInfo),
            .slotState = std::move(slotState),
        };
    }

    AddEdgeResponse addEdgeImpl(const AddEdgeRequest& request) {
        auto edgeInfoOr = builder_.AddEdge(request.sourceNode, request.sourceSlot, request.targetNode, request.targetSlot);
        if (!edgeInfoOr.ok()) {
            LOG(FATAL) << "Add edge error: " << edgeInfoOr.status();
        }
        const grph::EdgeInfo edgeInfo = std::move(edgeInfoOr).value();

        auto state0Or = GraphUtils::LookupSlotStates(state_, { SlotId{edgeInfo.node0, edgeInfo.slot0} });
        auto state1Or = GraphUtils::LookupSlotStates(state_, { SlotId{edgeInfo.node1, edgeInfo.slot1} });
        if (!state0Or.ok() || state0Or->size() != 1) {
            LOG(FATAL) << "Lookup source slot state error: " << state0Or.status();
        }
        if (!state1Or.ok() || state1Or->size() != 1) {
            LOG(FATAL) << "Lookup target slot state error: " << state1Or.status();
        }

        grph::SlotState& sourceState = std::move(state0Or).value().begin()->second;
        grph::SlotState& targetState = std::move(state1Or).value().begin()->second;
        sourceState.outEdges.insert(edgeInfo.id);
        targetState.inEdges.insert(edgeInfo.id);

        return AddEdgeResponse {
            .edgeInfo = std::move(edgeInfo),
            .sourceState = sourceState,
            .targetState = targetState,
        };
    }

    DeleteElementsResponse deleteElementsImpl(const DeleteElementsRequest& request) {
        auto deleteResultOr = builder_.DeleteElements(request.nodeIds, request.edgeIds);
        if (!deleteResultOr.ok()) {
            LOG(FATAL) << "Delete elements error: " << deleteResultOr.status();
        }

        std::vector<EdgeId> deletedEdgeIds;
        std::set<SlotId> deletedSlotIds;
        std::set<SlotId> affectedSlotIds;
        std::tie(deletedEdgeIds, deletedSlotIds, affectedSlotIds) = std::move(deleteResultOr).value();

        return DeleteElementsResponse {
            .nodeIds = request.nodeIds,
            .edgeIds = std::move(deletedEdgeIds),
            .deletedSlotIds = std::vector<SlotId>(deletedSlotIds.begin(), deletedSlotIds.end()),
            .affectedSlotIds = std::vector<SlotId>(affectedSlotIds.begin(), affectedSlotIds.end()),
        };
    }

    ValidateEdgeResponse validateEdgeImpl(const ValidateEdgeRequest& request) {
        SlotId sourceSlotId{request.sourceNode, request.sourceSlot};
        SlotId targetSlotId{request.targetNode, request.targetSlot};
        auto validateResultOr = builder_.ValidateEdge(sourceSlotId, targetSlotId);
        if (!validateResultOr.ok()) {
            LOG(FATAL) << "Validate edge error: " << validateResultOr.status();
        }
        grph::SlotState::Validity validity = std::move(validateResultOr).value();
        return ValidateEdgeResponse {
            .validity = validity,
        };
    }

    GetNodeStatesResponse getNodeStatesImpl(const GetNodeStatesRequest& request) {
        auto nodeStates = GraphUtils::LookupNodeStates(state_, request.nodeIds);
        if (!nodeStates.ok()) {
            LOG(FATAL) << "Lookup node states error: " << nodeStates.status();
        }
        std::vector<std::pair<NodeId, grph::NodeState>> result;
        for (auto& [nodeId, nodeState] : std::move(nodeStates).value()) {
            result.push_back({nodeId, std::move(nodeState)});
        }
        return GetNodeStatesResponse {
            .nodeStates = std::move(result),
        };
    }

    GetSlotStatesResponse getSlotStatesImpl(const GetSlotStatesRequest& request) {
        auto slotStates = GraphUtils::LookupSlotStates(state_, request.slotIds);
        if (!slotStates.ok()) {
            LOG(FATAL) << "Lookup slot states error: " << slotStates.status();
        }
        std::vector<std::pair<SlotId, grph::SlotState>> result;
        for (auto& [slotId, slotState] : std::move(slotStates).value()) {
            result.push_back({slotId, std::move(slotState)});
        }
        return GetSlotStatesResponse {
            .slotStates = std::move(result),
        };
    }

    VoidType clearGraphImpl(const VoidType&) {
        auto countOr = builder_.ClearGraph();
        if (!countOr.ok()) {
            LOG(FATAL) << "Clear graph error: " << countOr.status();
        }
        return {};
    }

    GetAvailableFuncsResponse getAvailableFuncsImpl(const GetAvailableFuncsRequest& request) {
        using CategoryFilter = GetAvailableFuncsRequest::CategoryFilter;
        FunctionLookup lookup;

        if (HasFilter(request.filters, CategoryFilter::URI_LIST)) {
            CHECK(request.uriList.has_value());
            lookup.WithUriList(request.uriList.value());
        }
        if (HasFilter(request.filters, CategoryFilter::PREFIX)) {
            CHECK(request.prefix.has_value());
            lookup.WithUriPrefix(request.prefix.value());
        }
        if (HasFilter(request.filters, CategoryFilter::PAGE)) {
            CHECK(request.pageStart.has_value());
            CHECK(request.pageSize.has_value());
            const int32_t start = std::max<int32_t>(0, request.pageStart.value());
            const int32_t size = std::max<int32_t>(0, request.pageSize.value());
            lookup.WithPagination(static_cast<size_t>(start), static_cast<size_t>(size));
        }

        std::vector<FunctionSpec> specs = lookup.Fetch();
        std::vector<FunctionInfo> infos;
        infos.reserve(specs.size());
        for (const FunctionSpec& spec : specs) {
            infos.push_back(ToFunctionInfo(spec));
        }

        return GetAvailableFuncsResponse {
            .infos = std::move(infos),
        };
    }

    VoidType setEncodedDataImpl(const SetEncodedDataRequest& request) {
        if (request.isNode) {
            if (request.nodeId.has_value() == false) {
                LOG(FATAL) << "Node id must be set for node encoded data.";
            }
            const NodeId nodeId = request.nodeId.value();
            auto status = builder_.SetNodeEncodedData(nodeId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set node encoded data error: " << status;
            }
        } else {
            if (request.slotId.has_value() == false) {
                LOG(FATAL) << "Slot id must be set for slot encoded data.";
            }
            const SlotId slotId = request.slotId.value();
            auto status = builder_.SetSlotEncodedData(slotId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set slot encoded data error: " << status;
            }
        }
        return {};
    }

    BuildPipelineResponse buildPipelineImpl(const VoidType&) {
        auto assetInfosOr = runner_.RebuildFromState(state_);
        if (!assetInfosOr.ok()) {
            LOG(FATAL) << "Build pipeline error: " << assetInfosOr.status();
        }
        return BuildPipelineResponse {
            .assetInfos = std::move(assetInfosOr).value(),
        };
    }

    RunPipelineResponse runPipelineImpl(const VoidType& request) {
        RunPipelineResponse response;
        auto runResult = runner_.RunPipeline();
        if (!runResult.ok()) {
            LOG(FATAL) << "Run pipeline error: " << runResult.status();
        }
        response.outputs = std::move(runResult).value();
        return response;
    }

    GetResourcesResponse getResourcesImpl(const VoidType&) {
        auto resourcesOr = runner_.GetPipelineResources();
        if (!resourcesOr.ok()) {
            LOG(FATAL) << "Get pipeline resources error: " << resourcesOr.status();
        }
        return GetResourcesResponse {
            .resources = std::move(resourcesOr).value(),
        };
    }

 private:
   GraphState state_;
   TopoSorter topoSorter_;
   GraphBuilder builder_;
   PipelineRunner runner_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphEngineApiBackend();
    GraphEngineApi::ImplPtrs<GraphEngineApiBackend> ptrs = {
        .getGraph = &GraphEngineApiBackend::getGraphImpl,
        .encodeGraph = &GraphEngineApiBackend::encodeGraphImpl,
        .decodeGraph = &GraphEngineApiBackend::decodeGraphImpl,
        .createNode = &GraphEngineApiBackend::createNodeImpl,
        .createIONode = &GraphEngineApiBackend::createIONodeImpl,
        .addEdge = &GraphEngineApiBackend::addEdgeImpl,
        .deleteElements = &GraphEngineApiBackend::deleteElementsImpl,
        .validateEdge = &GraphEngineApiBackend::validateEdgeImpl,
        .getNodeStates = &GraphEngineApiBackend::getNodeStatesImpl,
        .getSlotStates = &GraphEngineApiBackend::getSlotStatesImpl,
        .clearGraph = &GraphEngineApiBackend::clearGraphImpl,
        .getAvailableFuncs = &GraphEngineApiBackend::getAvailableFuncsImpl,
        .setEncodedData = &GraphEngineApiBackend::setEncodedDataImpl,
        .buildPipeline = &GraphEngineApiBackend::buildPipelineImpl,
        .runPipeline = &GraphEngineApiBackend::runPipelineImpl,
        .getResources = &GraphEngineApiBackend::getResourcesImpl,
    };
    cppschema::RegisterBackend<GraphEngineApi, GraphEngineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
