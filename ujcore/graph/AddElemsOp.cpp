#include "ujcore/graph/AddElemsOp.h"

#include <vector>
#include <any>
#include <optional>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/data/graph/AbslStringifies.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"
#include "ujcore/data/utils/CreateNodeSlots.h"
#include "ujcore/graph/IdGenerator.h"

namespace ujcore {

absl::StatusOr<data::GraphNode> AddElemsOp_CreateNode(GraphOpsContext& ctx, const data::FunctionInfo& fn_info) {
    const GraphConfig& config = ctx.config;
    GraphState& state = ctx.state;
    auto* const toposort_order = ctx.toposort_order;

    const uint32_t raw_id = ++(state.idgen_state.next_node_id);
    const std::string alphanum_id = GenSplitMix64OfLength(raw_id + config.nodeid_splitmix_offset, 10);

    auto slots_or = CreateNodeSlots(fn_info, raw_id, alphanum_id);
    if (!slots_or.ok()) {
        return std::move(slots_or).status();
    }
    std::vector<data::GraphSlot> new_slots = std::move(slots_or).value();

    std::vector<std::string> slot_names;
    slot_names.reserve(new_slots.size());
    for (const auto& slot : new_slots) {
        slot_names.push_back(slot.name);
        state.slots[std::make_tuple(slot.parent, slot.name)] = std::move(slot);
    }

    data::GraphNode graph_node = {
        .id = raw_id,
        .alnumid = alphanum_id,
        .fnuri = fn_info.uri,
        .slots = slot_names,
    };

    state.nodes_map[raw_id] = graph_node;
    state.node_id_lookup[alphanum_id] = raw_id;
    // toposort_order->AddNode(alphanum_id);
    return graph_node;
}

absl::StatusOr<std::vector<data::GraphEdge>> AddElemsOp_AddEdges(GraphOpsContext& ctx, const std::vector<data::AddEdgeEntry>& entries) {
    GraphState& state = ctx.state;
    const auto& slots = state.slots;
    std::vector<data::GraphEdge> edges;
    for (const auto& entry : entries) {
        const uint32_t node0_id = state.node_id_lookup[entry.node0];
        const uint32_t node1_id = state.node_id_lookup[entry.node1];
        if (node0_id == 0 || node1_id == 0) {
            return absl::InternalError("Node id lookup failed: " + entry.node0 + ", " + entry.node1);
        }
        auto iter0 = slots.find(std::make_tuple(node0_id, entry.slot0));
        auto iter1 = slots.find(std::make_tuple(node1_id, entry.slot1));
        if (iter0 == slots.end() || iter1 == slots.end()) {
            return absl::InternalError("Slot lookup failed, in node# " + entry.node0);
        }
        const uint32_t raw_id = static_cast<uint32_t>(++state.idgen_state.next_edge_id);
        const auto new_edge = data::GraphEdge {
            .id = raw_id,
            .node0 = node0_id,
            .node1 = node1_id,
            .slot0 = entry.slot0,
            .slot1 = entry.slot1,
        };
        state.edges_map[raw_id] = new_edge;
        edges.push_back(new_edge);
    }
    return edges;
}

}  // namespace ujcore
