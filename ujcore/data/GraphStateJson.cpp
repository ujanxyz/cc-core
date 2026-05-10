#include "ujcore/data/GraphStateJson.h"

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/TopoSortState.h"
#include "ujcore/utils/status_macros.h"

#include <optional>
#include <set>
#include <string>

namespace ujcore {
namespace {

using ::nlohmann::json;

std::string ToString(const plinfo::SlotInfo::AccessEnum access) {
    switch (access) {
        case plinfo::SlotInfo::AccessEnum::I:
            return "I";
        case plinfo::SlotInfo::AccessEnum::O:
            return "O";
        case plinfo::SlotInfo::AccessEnum::M:
            return "M";
    }
    CHECK(false) << "Unhandled SlotInfo::AccessEnum";
}

absl::StatusOr<plinfo::SlotInfo::AccessEnum> AccessEnumFromString(const std::string& encoded) {
    if (encoded == "I") {
        return plinfo::SlotInfo::AccessEnum::I;
    }
    if (encoded == "O") {
        return plinfo::SlotInfo::AccessEnum::O;
    }
    if (encoded == "M") {
        return plinfo::SlotInfo::AccessEnum::M;
    }
    return absl::InvalidArgumentError("Unknown SlotInfo::AccessEnum value");
}

std::string ToString(const plinfo::NodeInfo::NodeType ntype) {
    switch (ntype) {
        case plinfo::NodeInfo::NodeType::FN:
            return "FN";
        case plinfo::NodeInfo::NodeType::IN:
            return "IN";
        case plinfo::NodeInfo::NodeType::OUT:
            return "OUT";
    }
    CHECK(false) << "Unhandled NodeInfo::NodeType";
}

absl::StatusOr<plinfo::NodeInfo::NodeType> NodeTypeFromString(const std::string& encoded) {
    if (encoded == "FN") {
        return plinfo::NodeInfo::NodeType::FN;
    }
    if (encoded == "IN") {
        return plinfo::NodeInfo::NodeType::IN;
    }
    if (encoded == "OUT") {
        return plinfo::NodeInfo::NodeType::OUT;
    }
    return absl::InvalidArgumentError("Unknown NodeInfo::NodeType value");
}

std::string ToString(const plstate::NodeState::ConnectedState connected) {
    switch (connected) {
        case plstate::NodeState::ConnectedState::WAIT:
            return "WAIT";
        case plstate::NodeState::ConnectedState::RUN:
            return "RUN";
        case plstate::NodeState::ConnectedState::ERR:
            return "ERR";
    }
    CHECK(false) << "Unhandled NodeState::ConnectedState";
}

absl::StatusOr<plstate::NodeState::ConnectedState> ConnectedStateFromString(const std::string& encoded) {
    if (encoded == "WAIT") {
        return plstate::NodeState::ConnectedState::WAIT;
    }
    if (encoded == "RUN") {
        return plstate::NodeState::ConnectedState::RUN;
    }
    if (encoded == "ERR") {
        return plstate::NodeState::ConnectedState::ERR;
    }
    return absl::InvalidArgumentError("Unknown NodeState::ConnectedState value");
}

json EncodeNodeId(const NodeId nodeId) {
    return nodeId.value;
}

json EncodeEdgeId(const EdgeId edgeId) {
    return edgeId.value;
}

NodeId DecodeNodeId(const json& j) {
    return NodeId(j.get<uint32_t>());
}

EdgeId DecodeEdgeId(const json& j) {
    return EdgeId(j.get<uint32_t>());
}

json EncodeSlotId(const SlotId& slotId) {
    return json::array({slotId.parent.value, slotId.name});
}

absl::StatusOr<SlotId> DecodeSlotId(const json& j) {
    if (!j.is_array() || j.size() != 2 || !j[0].is_number_unsigned() || !j[1].is_string()) {
        return absl::InvalidArgumentError("SlotId must be [number, string]");
    }

    SlotId slotId;
    slotId.parent = NodeId(j[0].get<uint32_t>());
    slotId.name = j[1].get<std::string>();
    return slotId;
}

json EncodeEncodedData(const std::optional<plstate::EncodedData>& encodedData) {
    if (!encodedData.has_value()) {
        return nullptr;
    }
    return json{{"payload", encodedData->payload}};
}

absl::StatusOr<std::optional<plstate::EncodedData>> DecodeEncodedDataOptional(const json& j) {
    if (j.is_null()) {
        return std::nullopt;
    }
    if (!j.is_object()) {
        return absl::InvalidArgumentError("encodedData must be null or object");
    }
    if (!j.contains("payload") || !j["payload"].is_string()) {
        return absl::InvalidArgumentError("encodedData must have string payload");
    }

    return plstate::EncodedData{
        .payload = j["payload"].get<std::string>(),
    };
}

json EncodeSlotInfo(const plinfo::SlotInfo& slotInfo) {
    return json{
        {"parent", slotInfo.parent.value},
        {"name", slotInfo.name},
        {"dtype", slotInfo.dtype},
        {"access", ToString(slotInfo.access)},
    };
}

absl::StatusOr<plinfo::SlotInfo> DecodeSlotInfo(const json& j) {
    if (!j.is_object()) {
        return absl::InvalidArgumentError("SlotInfo must be an object");
    }

    if (!j.contains("parent") || !j["parent"].is_number_unsigned()) {
        return absl::InvalidArgumentError("SlotInfo missing or invalid parent");
    }
    if (!j.contains("name") || !j["name"].is_string()) {
        return absl::InvalidArgumentError("SlotInfo missing or invalid name");
    }
    if (!j.contains("dtype") || !j["dtype"].is_string()) {
        return absl::InvalidArgumentError("SlotInfo missing or invalid dtype");
    }
    if (!j.contains("access") || !j["access"].is_string()) {
        return absl::InvalidArgumentError("SlotInfo missing or invalid access");
    }

    plinfo::SlotInfo slotInfo;
    slotInfo.parent = NodeId(j["parent"].get<uint32_t>());
    slotInfo.name = j["name"].get<std::string>();
    slotInfo.dtype = j["dtype"].get<std::string>();
    ASSIGN_OR_RETURN(slotInfo.access, AccessEnumFromString(j["access"].get<std::string>()));
    return slotInfo;
}

json EncodeNodeInfo(const plinfo::NodeInfo& nodeInfo) {
    json result{
        {"rawId", nodeInfo.rawId.value},
        {"alnumid", nodeInfo.alnumid},
        {"ntype", ToString(nodeInfo.ntype)},
        {"uri", nodeInfo.uri},
        {"ins", nodeInfo.ins},
        {"outs", nodeInfo.outs},
        {"inouts", nodeInfo.inouts},
    };
    if (nodeInfo.ioDtype.has_value()) {
        result["ioDtype"] = *nodeInfo.ioDtype;
    }
    return result;
}

absl::StatusOr<plinfo::NodeInfo> DecodeNodeInfo(const json& j) {
    if (!j.is_object()) {
        return absl::InvalidArgumentError("NodeInfo must be an object");
    }

    if (!j.contains("rawId") || !j["rawId"].is_number_unsigned()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid rawId");
    }
    if (!j.contains("alnumid") || !j["alnumid"].is_string()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid alnumid");
    }
    if (!j.contains("ntype") || !j["ntype"].is_string()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid ntype");
    }
    if (!j.contains("uri") || !j["uri"].is_string()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid uri");
    }
    if (!j.contains("ins") || !j["ins"].is_array()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid ins");
    }
    if (!j.contains("outs") || !j["outs"].is_array()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid outs");
    }
    if (!j.contains("inouts") || !j["inouts"].is_array()) {
        return absl::InvalidArgumentError("NodeInfo missing or invalid inouts");
    }

    plinfo::NodeInfo nodeInfo;
    nodeInfo.rawId = NodeId(j["rawId"].get<uint32_t>());
    nodeInfo.alnumid = j["alnumid"].get<std::string>();
    ASSIGN_OR_RETURN(nodeInfo.ntype, NodeTypeFromString(j["ntype"].get<std::string>()));
    nodeInfo.uri = j["uri"].get<std::string>();
    if (j.contains("ioDtype") && !j["ioDtype"].is_null()) {
        nodeInfo.ioDtype = j["ioDtype"].get<std::string>();
    }
    nodeInfo.ins = j["ins"].get<std::vector<std::string>>();
    nodeInfo.outs = j["outs"].get<std::vector<std::string>>();
    nodeInfo.inouts = j["inouts"].get<std::vector<std::string>>();
    return nodeInfo;
}

json EncodeEdgeInfo(const plinfo::EdgeInfo& edgeInfo) {
    return json{
        {"id", edgeInfo.id.value},
        {"catid", edgeInfo.catid},
        {"node0", edgeInfo.node0.value},
        {"node1", edgeInfo.node1.value},
        {"slot0", edgeInfo.slot0},
        {"slot1", edgeInfo.slot1},
    };
}

absl::StatusOr<plinfo::EdgeInfo> DecodeEdgeInfo(const json& j) {
    if (!j.is_object()) {
        return absl::InvalidArgumentError("EdgeInfo must be an object");
    }

    if (!j.contains("id") || !j["id"].is_number_unsigned()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid id");
    }
    if (!j.contains("catid") || !j["catid"].is_string()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid catid");
    }
    if (!j.contains("node0") || !j["node0"].is_number_unsigned()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid node0");
    }
    if (!j.contains("node1") || !j["node1"].is_number_unsigned()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid node1");
    }
    if (!j.contains("slot0") || !j["slot0"].is_string()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid slot0");
    }
    if (!j.contains("slot1") || !j["slot1"].is_string()) {
        return absl::InvalidArgumentError("EdgeInfo missing or invalid slot1");
    }

    return plinfo::EdgeInfo{
        .id = EdgeId(j["id"].get<uint32_t>()),
        .catid = j["catid"].get<std::string>(),
        .node0 = NodeId(j["node0"].get<uint32_t>()),
        .node1 = NodeId(j["node1"].get<uint32_t>()),
        .slot0 = j["slot0"].get<std::string>(),
        .slot1 = j["slot1"].get<std::string>(),
    };
}

json EncodeSlotState(const plstate::SlotState& slotState) {
    json inEdges = json::array();
    for (const EdgeId edgeId : slotState.inEdges) {
        inEdges.push_back(EncodeEdgeId(edgeId));
    }

    json outEdges = json::array();
    for (const EdgeId edgeId : slotState.outEdges) {
        outEdges.push_back(EncodeEdgeId(edgeId));
    }

    json result{
        {"inEdges", inEdges},
        {"outEdges", outEdges},
        {"genId", slotState.genId},
    };
    if (slotState.encodedData.has_value()) {
        result["encodedData"] = EncodeEncodedData(slotState.encodedData);
    }
    return result;
}

absl::StatusOr<plstate::SlotState> DecodeSlotState(const json& j) {
    if (!j.is_object()) {
        return absl::InvalidArgumentError("SlotState must be an object");
    }

    if (!j.contains("inEdges") || !j["inEdges"].is_array()) {
        return absl::InvalidArgumentError("SlotState missing or invalid inEdges");
    }
    if (!j.contains("outEdges") || !j["outEdges"].is_array()) {
        return absl::InvalidArgumentError("SlotState missing or invalid outEdges");
    }
    if (!j.contains("genId") || !j["genId"].is_number_integer()) {
        return absl::InvalidArgumentError("SlotState missing or invalid genId");
    }

    plstate::SlotState slotState;
    for (const json& edgeIdJson : j["inEdges"]) {
        slotState.inEdges.insert(DecodeEdgeId(edgeIdJson));
    }
    for (const json& edgeIdJson : j["outEdges"]) {
        slotState.outEdges.insert(DecodeEdgeId(edgeIdJson));
    }
    slotState.genId = j["genId"].get<int32_t>();
    if (j.contains("encodedData")) {
        ASSIGN_OR_RETURN(slotState.encodedData, DecodeEncodedDataOptional(j["encodedData"]));
    }
    return slotState;
}

json EncodeNodeState(const plstate::NodeState& nodeState) {
    json result{
        {"label", nodeState.label},
        {"connected", ToString(nodeState.connected)},
        {"genId", nodeState.genId},
    };
    if (nodeState.encodedData.has_value()) {
        result["encodedData"] = EncodeEncodedData(nodeState.encodedData);
    }
    return result;
}

absl::StatusOr<plstate::NodeState> DecodeNodeState(const json& j) {
    if (!j.is_object()) {
        return absl::InvalidArgumentError("NodeState must be an object");
    }

    if (!j.contains("label") || !j["label"].is_string()) {
        return absl::InvalidArgumentError("NodeState missing or invalid label");
    }
    if (!j.contains("connected") || !j["connected"].is_string()) {
        return absl::InvalidArgumentError("NodeState missing or invalid connected");
    }
    if (!j.contains("genId") || !j["genId"].is_number_integer()) {
        return absl::InvalidArgumentError("NodeState missing or invalid genId");
    }

    plstate::NodeState nodeState;
    nodeState.label = j["label"].get<std::string>();
    ASSIGN_OR_RETURN(nodeState.connected, ConnectedStateFromString(j["connected"].get<std::string>()));
    nodeState.genId = j["genId"].get<int32_t>();
    if (j.contains("encodedData")) {
        ASSIGN_OR_RETURN(nodeState.encodedData, DecodeEncodedDataOptional(j["encodedData"]));
    }
    return nodeState;
}

json EncodeSlotInfoMap(const std::map<SlotId, plinfo::SlotInfo>& slotInfos) {
    json entries = json::array();
    for (const auto& [key, value] : slotInfos) {
        entries.push_back(json::array({EncodeSlotId(key), EncodeSlotInfo(value)}));
    }
    return entries;
}

json EncodeNodeInfoMap(const std::map<NodeId, plinfo::NodeInfo>& nodeInfos) {
    json entries = json::array();
    for (const auto& [key, value] : nodeInfos) {
        entries.push_back(json::array({EncodeNodeId(key), EncodeNodeInfo(value)}));
    }
    return entries;
}

json EncodeEdgeInfoMap(const std::map<EdgeId, plinfo::EdgeInfo>& edgeInfos) {
    json entries = json::array();
    for (const auto& [key, value] : edgeInfos) {
        entries.push_back(json::array({EncodeEdgeId(key), EncodeEdgeInfo(value)}));
    }
    return entries;
}

json EncodeSlotStateMap(const std::map<SlotId, plstate::SlotState>& slotStates) {
    json entries = json::array();
    for (const auto& [key, value] : slotStates) {
        entries.push_back(json::array({EncodeSlotId(key), EncodeSlotState(value)}));
    }
    return entries;
}

json EncodeNodeStateMap(const std::map<NodeId, plstate::NodeState>& nodeStates) {
    json entries = json::array();
    for (const auto& [key, value] : nodeStates) {
        entries.push_back(json::array({EncodeNodeId(key), EncodeNodeState(value)}));
    }
    return entries;
}

} // namespace

absl::StatusOr<std::string> EncodeGraphState(const GraphState& state) {
    json j;

    j["idgenState"] = {
        {"nextNodeId", state.idgen_state.next_node_id},
        {"nextEdgeId", state.idgen_state.next_edge_id},
    };

    json topoSortState;
    topoSortState["sortOrder"] = json::array();
    for (const NodeId nodeId : state.topoSortState.sortOrder) {
        topoSortState["sortOrder"].push_back(EncodeNodeId(nodeId));
    }
    topoSortState["hasDirtyBit"] = state.topoSortState.hasDirtyBit;
    j["topoSortState"] = topoSortState;

    j["slotInfos"] = EncodeSlotInfoMap(state.slotInfos);
    j["nodeInfos"] = EncodeNodeInfoMap(state.nodeInfos);
    j["edgeInfos"] = EncodeEdgeInfoMap(state.edgeInfos);
    j["slotStates"] = EncodeSlotStateMap(state.slotStates);
    j["nodeStates"] = EncodeNodeStateMap(state.nodeStates);

    std::string result = j.dump();
    return result;
}

absl::StatusOr<GraphState> DecodeGraphState(const std::string& encoded) {
    GraphState state;

    json j;
    if (!json::accept(encoded)) {
        return absl::InvalidArgumentError("Invalid JSON");
    }
    j = json::parse(encoded, nullptr, false);
    if (j.is_discarded()) {
        return absl::InvalidArgumentError("Failed to parse JSON");
    }

    if (j.contains("idgenState")) {
        const json& idgenState = j["idgenState"];
        if (!idgenState.is_object()) {
            return absl::InvalidArgumentError("idgenState must be object");
        }
        if (idgenState.contains("nextNodeId")) {
            if (!idgenState["nextNodeId"].is_number_unsigned()) {
                return absl::InvalidArgumentError("nextNodeId must be unsigned integer");
            }
            state.idgen_state.next_node_id = idgenState["nextNodeId"].get<uint32_t>();
        }
        if (idgenState.contains("nextEdgeId")) {
            if (!idgenState["nextEdgeId"].is_number_unsigned()) {
                return absl::InvalidArgumentError("nextEdgeId must be unsigned integer");
            }
            state.idgen_state.next_edge_id = idgenState["nextEdgeId"].get<uint32_t>();
        }
    }

    if (j.contains("topoSortState")) {
        const json& topoSortState = j["topoSortState"];
        if (!topoSortState.is_object()) {
            return absl::InvalidArgumentError("topoSortState must be object");
        }
        if (topoSortState.contains("hasDirtyBit")) {
            if (!topoSortState["hasDirtyBit"].is_boolean()) {
                return absl::InvalidArgumentError("hasDirtyBit must be boolean");
            }
            state.topoSortState.hasDirtyBit = topoSortState["hasDirtyBit"].get<bool>();
        }
        if (topoSortState.contains("sortOrder")) {
            const json& sortOrderJson = topoSortState["sortOrder"];
            if (!sortOrderJson.is_array()) {
                return absl::InvalidArgumentError("sortOrder must be array");
            }
            for (const json& nodeIdJson : sortOrderJson) {
                state.topoSortState.sortOrder.push_back(DecodeNodeId(nodeIdJson));
            }
        }
    }

    if (j.contains("slotInfos")) {
        const json& slotInfosJson = j["slotInfos"];
        if (!slotInfosJson.is_array()) {
            return absl::InvalidArgumentError("slotInfos must be array");
        }
        for (const json& entry : slotInfosJson) {
            if (!entry.is_array() || entry.size() != 2) {
                return absl::InvalidArgumentError("slotInfos entries must be [key, value]");
            }
            ASSIGN_OR_RETURN(const SlotId key, DecodeSlotId(entry[0]));
            ASSIGN_OR_RETURN(const plinfo::SlotInfo value, DecodeSlotInfo(entry[1]));
            state.slotInfos[key] = value;
        }
    }

    if (j.contains("nodeInfos")) {
        const json& nodeInfosJson = j["nodeInfos"];
        if (!nodeInfosJson.is_array()) {
            return absl::InvalidArgumentError("nodeInfos must be array");
        }
        for (const json& entry : nodeInfosJson) {
            if (!entry.is_array() || entry.size() != 2) {
                return absl::InvalidArgumentError("nodeInfos entries must be [key, value]");
            }
            const NodeId key = DecodeNodeId(entry[0]);
            ASSIGN_OR_RETURN(const plinfo::NodeInfo value, DecodeNodeInfo(entry[1]));
            state.nodeInfos[key] = value;
        }
    }

    if (j.contains("edgeInfos")) {
        const json& edgeInfosJson = j["edgeInfos"];
        if (!edgeInfosJson.is_array()) {
            return absl::InvalidArgumentError("edgeInfos must be array");
        }
        for (const json& entry : edgeInfosJson) {
            if (!entry.is_array() || entry.size() != 2) {
                return absl::InvalidArgumentError("edgeInfos entries must be [key, value]");
            }
            const EdgeId key = DecodeEdgeId(entry[0]);
            ASSIGN_OR_RETURN(const plinfo::EdgeInfo value, DecodeEdgeInfo(entry[1]));
            state.edgeInfos[key] = value;
        }
    }

    if (j.contains("slotStates")) {
        const json& slotStatesJson = j["slotStates"];
        if (!slotStatesJson.is_array()) {
            return absl::InvalidArgumentError("slotStates must be array");
        }
        for (const json& entry : slotStatesJson) {
            if (!entry.is_array() || entry.size() != 2) {
                return absl::InvalidArgumentError("slotStates entries must be [key, value]");
            }
            ASSIGN_OR_RETURN(const SlotId key, DecodeSlotId(entry[0]));
            ASSIGN_OR_RETURN(const plstate::SlotState value, DecodeSlotState(entry[1]));
            state.slotStates[key] = value;
        }
    }

    if (j.contains("nodeStates")) {
        const json& nodeStatesJson = j["nodeStates"];
        if (!nodeStatesJson.is_array()) {
            return absl::InvalidArgumentError("nodeStates must be array");
        }
        for (const json& entry : nodeStatesJson) {
            if (!entry.is_array() || entry.size() != 2) {
                return absl::InvalidArgumentError("nodeStates entries must be [key, value]");
            }
            const NodeId key = DecodeNodeId(entry[0]);
            ASSIGN_OR_RETURN(const plstate::NodeState value, DecodeNodeState(entry[1]));
            state.nodeStates[key] = value;
        }
    }

    return state;
}

} // namespace ujcore