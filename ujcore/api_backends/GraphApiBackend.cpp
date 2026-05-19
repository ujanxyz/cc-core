#include "ujcore/api_schemas/GraphApi.h"

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/api_backends/SharedStore.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionLookup.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/graph/GraphUtils.h"
#include "ujcore/utils/IdUtils.h"

namespace ujcore {
namespace {

using GetGraphMetaResponse = GraphApi::GetGraphMetaResponse;
using SetGraphMetaRequest = GraphApi::SetGraphMetaRequest;
using GetGraphResponse = GraphApi::GetGraphResponse;
using CreateNodeRequest = GraphApi::CreateNodeRequest;
using CreateNodeResponse = GraphApi::CreateNodeResponse;
using CreateIONodeRequest = GraphApi::CreateIONodeRequest;
using CreateIONodeResponse = GraphApi::CreateIONodeResponse;
using AddEdgesRequest = GraphApi::AddEdgesRequest;
using AddEdgesResponse = GraphApi::AddEdgesResponse;
using DeleteElementsRequest = GraphApi::DeleteElementsRequest;
using DeleteElementsResponse = GraphApi::DeleteElementsResponse;
using ValidateEdgeRequest = GraphApi::ValidateEdgeRequest;
using ValidateEdgeResponse = GraphApi::ValidateEdgeResponse;
using GetNodeStatesRequest = GraphApi::GetNodeStatesRequest;
using GetNodeStatesResponse = GraphApi::GetNodeStatesResponse;
using GetSlotStatesRequest = GraphApi::GetSlotStatesRequest;
using GetSlotStatesResponse = GraphApi::GetSlotStatesResponse;
using GetAvailableFuncsRequest = GraphApi::GetAvailableFuncsRequest;
using GetAvailableFuncsResponse = GraphApi::GetAvailableFuncsResponse;
using SetEncodedDataRequest = GraphApi::SetEncodedDataRequest;

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

class GraphApiBackend : public cppschema::ApiBackend<GraphApi> {
 public:
    GraphApiBackend(): store_(SharedStore::Instance()) {}

    GetGraphMetaResponse getGraphMetaImpl(const VoidType& kvoid) {
        return GetGraphMetaResponse {
            .lastNodeId = store_.state().idgen_state.lastNodeId,
            .lastEdgeId = store_.state().idgen_state.lastEdgeId,
        };
    }

    VoidType setGraphMetaImpl(const SetGraphMetaRequest& request) {
        store_.state().idgen_state.lastNodeId = request.lastNodeId;
        store_.state().idgen_state.lastEdgeId = request.lastEdgeId;
        return VoidType{};
    }

    GetGraphResponse getGraphImpl(const VoidType& kvoid) {
        return GetGraphResponse {
            .nodeInfos = GraphUtils::GetAllNodeInfos(store_.state()),
            .edgeInfos = GraphUtils::GetAllEdgeInfos(store_.state()),
            .slotInfos = GraphUtils::GetAllSlotInfos(store_.state()),
        };
    }

    CreateNodeResponse createNodeImpl(const CreateNodeRequest& request) {
        std::optional<NodeId> overrideIdOpt;
        if (request.overrideId.has_value()) {
            overrideIdOpt = NodeId(DecodeStringId(request.overrideId.value()));
        }

        auto nodeInfoOr = store_.builder().AddFuncNode(request.func, overrideIdOpt);
        if (!nodeInfoOr.ok()) {
            LOG(FATAL) << "Insert node error: " << nodeInfoOr.status();
        }
        const NodeId nodeId = nodeInfoOr->rawId;
        const grph::NodeInfo nodeInfo = std::move(nodeInfoOr).value();

        auto addNodeSlotInfos = [this, nodeRawId = nodeId](const std::vector<std::string>& slotNames, std::vector<grph::SlotInfo>& result) -> void {
            auto slotsOr = store_.builder().LookupNodeSlotInfos(NodeId(nodeRawId), slotNames);
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
            auto slotStatesOr = GraphUtils::LookupSlotStates(store_.state(), slotIds);
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
        response.nodeState = GraphUtils::CopyNodeState(store_.state(), nodeId);
        return response;
    }

    CreateIONodeResponse createIONodeImpl(const CreateIONodeRequest& request) {
        std::optional<NodeId> overrideIdOpt;
        if (request.overrideId.has_value()) {
            overrideIdOpt = NodeId(DecodeStringId(request.overrideId.value()));
        }

        auto nodeSlotInfoOr = store_.builder().AddIONode(request.dtype, request.isOutput, overrideIdOpt);
        if (!nodeSlotInfoOr.ok()) {
            LOG(FATAL) << "Insert IO node error: " << nodeSlotInfoOr.status();
        }
        grph::NodeInfo nodeInfo;
        grph::SlotInfo slotInfo;
        std::tie(nodeInfo, slotInfo) = std::move(nodeSlotInfoOr).value();

        const NodeId nodeId = nodeInfo.rawId;
        auto slotStateOr = GraphUtils::LookupSlotStates(store_.state(), { SlotId{nodeId, slotInfo.name} });
        if (!slotStateOr.ok() || slotStateOr->size() != 1) {
            LOG(FATAL) << "Lookup IO node slot state error: " << slotStateOr.status();
        }
        const grph::SlotState slotState = std::move(slotStateOr).value().begin()->second;
        return CreateIONodeResponse {
            .nodeInfo = std::move(nodeInfo),
            .nodeState = GraphUtils::CopyNodeState(store_.state(), nodeId),
            .slotInfo = std::move(slotInfo),
            .slotState = std::move(slotState),
        };
    }

    AddEdgesResponse addEdgesImpl(const AddEdgesRequest& request) {
        std::vector<GraphBuilder::AddEdgeEntry> entries;
        entries.reserve(request.entries.size());
        for (const auto& entry : request.entries) {
            std::optional<EdgeId> overrideEdgeIdOpt;
            if (entry.overrideEdgeId.has_value()) {
                overrideEdgeIdOpt = EdgeId(entry.overrideEdgeId.value());
            }
            entries.push_back({
                .node0 = NodeId(DecodeStringId(entry.source.parent)),
                .slot0 = entry.source.name,
                .node1 = NodeId(DecodeStringId(entry.target.parent)),
                .slot1 = entry.target.name,
                .overrideEdgeId = overrideEdgeIdOpt,
            });
        }

        auto edgeInfosOr = store_.builder().AddEdges(entries);
        if (!edgeInfosOr.ok()) {
            LOG(FATAL) << "Add edges error: " << edgeInfosOr.status();
        }
        return AddEdgesResponse {
            .edgeInfos = std::move(edgeInfosOr).value(),
        };
    }

    DeleteElementsResponse deleteElementsImpl(const DeleteElementsRequest& request) {
        auto deleteResultOr = store_.builder().DeleteElements(request.nodeIds, request.edgeIds);
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
        auto validateResultOr = store_.builder().ValidateEdge(sourceSlotId, targetSlotId);
        if (!validateResultOr.ok()) {
            LOG(FATAL) << "Validate edge error: " << validateResultOr.status();
        }
        grph::SlotState::Validity validity = std::move(validateResultOr).value();
        return ValidateEdgeResponse {
            .validity = validity,
        };
    }

    GetNodeStatesResponse getNodeStatesImpl(const GetNodeStatesRequest& request) {
        auto nodeStates = GraphUtils::LookupNodeStates(store_.state(), request.nodeIds);
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
        std::vector<SlotId> slotIds;
        for (const EncodedSlotId& encodedSlotId : request.slotIdsEncoded) {
            const NodeId nodeId = NodeId(DecodeStringId(encodedSlotId.parent));
            slotIds.push_back(SlotId{nodeId, encodedSlotId.name});
        }
        for (const SlotId& slotId : request.slotIds) {
            slotIds.push_back(slotId);
        }
        auto slotStates = GraphUtils::LookupSlotStates(store_.state(), slotIds);
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
        auto countOr = store_.builder().ClearGraph();
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
            // auto status = store_.builder().SetNodeEncodedData(nodeId, request.encodedData);
            // if (!status.ok()) {
            //     LOG(FATAL) << "Set node encoded data error: " << status;
            // }

            const SlotId slotId = {.parent = nodeId, .name = "$out"};
            auto status = store_.builder().SetSlotEncodedData(slotId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set slot encoded data error: " << status;
            }

        } else {
            if (request.slotId.has_value() == false) {
                LOG(FATAL) << "Slot id must be set for slot encoded data.";
            }
            const SlotId slotId = request.slotId.value();
            auto status = store_.builder().SetSlotEncodedData(slotId, request.encodedData);
            if (!status.ok()) {
                LOG(FATAL) << "Set slot encoded data error: " << status;
            }
        }
        return {};
    }

 private:
   SharedStore& store_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new GraphApiBackend();
    GraphApi::ImplPtrs<GraphApiBackend> ptrs = {
        .getGraphMeta = &GraphApiBackend::getGraphMetaImpl,
        .setGraphMeta = &GraphApiBackend::setGraphMetaImpl,
        .getGraph = &GraphApiBackend::getGraphImpl,
        .createNode = &GraphApiBackend::createNodeImpl,
        .createIONode = &GraphApiBackend::createIONodeImpl,
        .addEdges = &GraphApiBackend::addEdgesImpl,
        .deleteElements = &GraphApiBackend::deleteElementsImpl,
        .validateEdge = &GraphApiBackend::validateEdgeImpl,
        .getNodeStates = &GraphApiBackend::getNodeStatesImpl,
        .getSlotStates = &GraphApiBackend::getSlotStatesImpl,
        .clearGraph = &GraphApiBackend::clearGraphImpl,
        .getAvailableFuncs = &GraphApiBackend::getAvailableFuncsImpl,
        .setEncodedData = &GraphApiBackend::setEncodedDataImpl,
    };
    cppschema::RegisterBackend<GraphApi, GraphApiBackend>(impl, ptrs);
}

}  // namespace ujcore
