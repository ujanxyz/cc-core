#include "ujcore/data/GraphStateJson.h"

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "ujcore/data/GraphState.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"

namespace ujcore {
namespace {

GraphState MakeSampleGraphState() {
	GraphState state;
	state.idgen_state.next_node_id = 5;
	state.idgen_state.next_edge_id = 9;
	state.topoSortState.sortOrder = {NodeId(1), NodeId(2)};
	state.topoSortState.hasDirtyBit = true;

	const SlotId slotIn {.parent = NodeId(1), .name = "x"};
	const SlotId slotOut {.parent = NodeId(2), .name = "out"};

	state.slotInfos[slotIn] = plinfo::SlotInfo {
		.parent = NodeId(1),
		.name = "x",
		.dtype = "float",
		.access = plinfo::SlotInfo::AccessEnum::I,
	};
	state.slotInfos[slotOut] = plinfo::SlotInfo {
		.parent = NodeId(2),
		.name = "out",
		.dtype = "float",
		.access = plinfo::SlotInfo::AccessEnum::O,
	};

	state.nodeInfos[NodeId(1)] = plinfo::NodeInfo {
		.rawId = NodeId(1),
		.alnumid = "A",
		.ntype = plinfo::NodeInfo::NodeType::FN,
		.uri = "/fn/in",
		.ioDtype = std::nullopt,
		.ins = {"x"},
		.outs = {"out"},
		.inouts = {},
	};
	state.nodeInfos[NodeId(2)] = plinfo::NodeInfo {
		.rawId = NodeId(2),
		.alnumid = "B",
		.ntype = plinfo::NodeInfo::NodeType::OUT,
		.uri = "/$OUT/float",
		.ioDtype = std::optional<std::string>("float"),
		.ins = {"out"},
		.outs = {},
		.inouts = {},
	};

	state.edgeInfos[EdgeId(3)] = plinfo::EdgeInfo {
		.id = EdgeId(3),
		.catid = "1|3|x|2|out",
		.node0 = NodeId(1),
		.node1 = NodeId(2),
		.slot0 = "x",
		.slot1 = "out",
	};

	state.slotStates[slotIn] = plstate::SlotState {
		.inEdges = {},
		.outEdges = {EdgeId(3)},
		.encodedData = std::optional<plstate::EncodedData>(plstate::EncodedData{.payload = "42"}),
		.genId = 12,
	};

	state.nodeStates[NodeId(2)] = plstate::NodeState {
		.label = "Output",
		.encodedData = std::optional<plstate::EncodedData>(plstate::EncodedData{.payload = "3.14"}),
		.connected = plstate::NodeState::ConnectedState::RUN,
		.genId = 7,
	};

	return state;
}

void ExpectGraphStateEqual(const GraphState& lhs, const GraphState& rhs) {
	EXPECT_EQ(lhs.idgen_state.next_node_id, rhs.idgen_state.next_node_id);
	EXPECT_EQ(lhs.idgen_state.next_edge_id, rhs.idgen_state.next_edge_id);

	ASSERT_EQ(lhs.topoSortState.sortOrder.size(), rhs.topoSortState.sortOrder.size());
	for (size_t i = 0; i < lhs.topoSortState.sortOrder.size(); ++i) {
		EXPECT_EQ(lhs.topoSortState.sortOrder[i].value, rhs.topoSortState.sortOrder[i].value);
	}
	EXPECT_EQ(lhs.topoSortState.hasDirtyBit, rhs.topoSortState.hasDirtyBit);

	ASSERT_EQ(lhs.slotInfos.size(), rhs.slotInfos.size());
	for (const auto& [key, value] : lhs.slotInfos) {
		auto it = rhs.slotInfos.find(key);
		ASSERT_NE(it, rhs.slotInfos.end());
		EXPECT_EQ(value.parent.value, it->second.parent.value);
		EXPECT_EQ(value.name, it->second.name);
		EXPECT_EQ(value.dtype, it->second.dtype);
		EXPECT_EQ(value.access, it->second.access);
	}

	ASSERT_EQ(lhs.nodeInfos.size(), rhs.nodeInfos.size());
	for (const auto& [key, value] : lhs.nodeInfos) {
		auto it = rhs.nodeInfos.find(key);
		ASSERT_NE(it, rhs.nodeInfos.end());
		EXPECT_EQ(value.rawId.value, it->second.rawId.value);
		EXPECT_EQ(value.alnumid, it->second.alnumid);
		EXPECT_EQ(value.ntype, it->second.ntype);
		EXPECT_EQ(value.uri, it->second.uri);
		EXPECT_EQ(value.ioDtype, it->second.ioDtype);
		EXPECT_EQ(value.ins, it->second.ins);
		EXPECT_EQ(value.outs, it->second.outs);
		EXPECT_EQ(value.inouts, it->second.inouts);
	}

	ASSERT_EQ(lhs.edgeInfos.size(), rhs.edgeInfos.size());
	for (const auto& [key, value] : lhs.edgeInfos) {
		auto it = rhs.edgeInfos.find(key);
		ASSERT_NE(it, rhs.edgeInfos.end());
		EXPECT_EQ(value.id.value, it->second.id.value);
		EXPECT_EQ(value.catid, it->second.catid);
		EXPECT_EQ(value.node0.value, it->second.node0.value);
		EXPECT_EQ(value.node1.value, it->second.node1.value);
		EXPECT_EQ(value.slot0, it->second.slot0);
		EXPECT_EQ(value.slot1, it->second.slot1);
	}

	ASSERT_EQ(lhs.slotStates.size(), rhs.slotStates.size());
	for (const auto& [key, value] : lhs.slotStates) {
		auto it = rhs.slotStates.find(key);
		ASSERT_NE(it, rhs.slotStates.end());
		EXPECT_EQ(value.inEdges, it->second.inEdges);
		EXPECT_EQ(value.outEdges, it->second.outEdges);
		ASSERT_EQ(value.encodedData.has_value(), it->second.encodedData.has_value());
		if (value.encodedData.has_value()) {
			EXPECT_EQ(value.encodedData->payload, it->second.encodedData->payload);
		}
		EXPECT_EQ(value.genId, it->second.genId);
	}

	ASSERT_EQ(lhs.nodeStates.size(), rhs.nodeStates.size());
	for (const auto& [key, value] : lhs.nodeStates) {
		auto it = rhs.nodeStates.find(key);
		ASSERT_NE(it, rhs.nodeStates.end());
		EXPECT_EQ(value.label, it->second.label);
		ASSERT_EQ(value.encodedData.has_value(), it->second.encodedData.has_value());
		if (value.encodedData.has_value()) {
			EXPECT_EQ(value.encodedData->payload, it->second.encodedData->payload);
		}
		EXPECT_EQ(value.connected, it->second.connected);
		EXPECT_EQ(value.genId, it->second.genId);
	}
}

}  // namespace

TEST(GraphStateJsonTest, StructToJsonToStructRoundTrip) {
	const GraphState original = MakeSampleGraphState();
	ASSERT_TRUE(EncodeGraphState(original).ok());

	const std::string encoded = *EncodeGraphState(original);
	const absl::StatusOr<GraphState> decodedOr = DecodeGraphState(encoded);

	ASSERT_TRUE(decodedOr.ok()) << decodedOr.status();
	ExpectGraphStateEqual(original, *decodedOr);
}

TEST(GraphStateJsonTest, JsonToStructToJsonRoundTrip) {
	const std::string input = R"json(
	{
	  "idgenState": {"nextNodeId": 11, "nextEdgeId": 17},
	  "topoSortState": {"sortOrder": [1, 2], "hasDirtyBit": true},
	  "slotInfos": [
		[[1, "x"], {"parent": 1, "name": "x", "dtype": "float", "access": "I"}]
	  ],
	  "nodeInfos": [
		[1, {"rawId": 1, "alnumid": "A", "ntype": "FN", "uri": "/fn/add", "ins": ["x"], "outs": ["out"], "inouts": []}]
	  ],
	  "edgeInfos": [
		[5, {"id": 5, "catid": "1|5|out|2|in", "node0": 1, "node1": 2, "slot0": "out", "slot1": "in"}]
	  ],
	  "slotStates": [
				[[1, "x"], {"inEdges": [], "outEdges": [5], "encodedData": {"payload": "abc"}, "genId": 4}]
	  ],
	  "nodeStates": [
		[1, {"label": "node-1", "encodedData": {"payload": "xyz"}, "connected": "WAIT", "genId": 9}]
	  ]
	})json";

	const absl::StatusOr<GraphState> decodedOr = DecodeGraphState(input);
	ASSERT_TRUE(decodedOr.ok()) << decodedOr.status();

	const absl::StatusOr<std::string> reEncodedOr = EncodeGraphState(*decodedOr);
	ASSERT_TRUE(reEncodedOr.ok()) << reEncodedOr.status();

	const nlohmann::json inputJson = nlohmann::json::parse(input);
	const nlohmann::json reEncodedJson = nlohmann::json::parse(*reEncodedOr);
	EXPECT_EQ(inputJson, reEncodedJson);
}

} // namespace ujcore
