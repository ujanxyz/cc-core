#include "ujcore/graph/AddElemsOp.h"

#include <vector>
#include <any>
#include <optional>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/data/graph/AbslStringifies.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"
#include "ujcore/graph/IdGenerator.h"

namespace ujcore {
namespace {

data::GraphSlot MakeSlotV2(const uint32_t parent, const std::string& name, const std::string& dtype, const bool isoutput) {
    return data::GraphSlot {
        .parent = parent,
        .name = name,
        .dtype = dtype,
        .isoutput = isoutput,
    };
}

/**
 * For a new node based on a function spec, construct its slots.
 * Each node type defines its slot topology and naming convention:
 *
 * - Graph Input ("in")
 *   - 1 output slot
 *   - "<id>$out"
 *
 * - Graph Output ("out")
 *   - 1 input slot
 *   - "<id>$in"
 *
 * - Function Node ("fn")
 *   - N input slots:  "<id>$in:<param-name>"
 *   - M output slots: "<id>$out:<param-name>"
 *
 * - Operator Node ("op")
 *   - 1 implicit input  slot: "<id>$in"
 *   - 1 implicit output slot: "<id>$out"
 *   - N named input slots:    "<id>$in:<param-name>"
 *
 * - Lambda Node ("lam")
 *   - 1 output slot (functor): "<id>$out"
 *   - N input slots:           "<id>$in:<param-name>"
 */
std::vector<data::GraphSlot> CreateNodeSlotsV2(const NodeFunctionSpec& func_spec, const uint32_t node_id, const std::string& node_alnumid) {
    const std::string prefix = "$";

    const auto in_id = [&](std::string_view name = "") {
        return name.empty()
            ? prefix + "in"
            : prefix + "in:" + std::string(name);
    };
    const auto out_id = [&](std::string_view name = "") {
        return name.empty()
            ? prefix + "out"
            : prefix + "out:" + std::string(name);
    };

    std::vector<data::GraphSlot> output;

    if (func_spec.kind == "in") {
        const auto& ext = std::any_cast<const InputNodeExt&>(func_spec.ext);

        output.push_back(
            MakeSlotV2(node_id, out_id(), ext.data_type, true));
    }
    else if (func_spec.kind == "out") {
        const auto& ext = std::any_cast<const OutputNodeExt&>(func_spec.ext);

        output.push_back(
            MakeSlotV2(node_id, in_id(), ext.data_type, false));
    }
    else if (func_spec.kind == "fn") {
        const auto& ext = std::any_cast<const FunctionNodeExt&>(func_spec.ext);

        // Function inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.data_type, false));
        }

        // Function outputs
        for (const auto& p : ext.outputs) {
            output.push_back(
                MakeSlotV2(node_id, out_id(p.name), p.data_type, true));
        }
    }
    else if (func_spec.kind == "op") {
        const auto& ext = std::any_cast<const OperatorNodeExt&>(func_spec.ext);

        // Implicit input/output
        output.push_back(
            MakeSlotV2(node_id, in_id(), ext.data_type, false));

        output.push_back(
            MakeSlotV2(node_id, out_id(), ext.data_type, true));

        // Named inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.data_type, false));
        }
    }
    else if (func_spec.kind == "lam") {
        const auto& ext = std::any_cast<const LambdaNodeExt&>(func_spec.ext);

        // Output (functor)
        output.push_back(
            MakeSlotV2(node_id, out_id(), ext.data_type, true));

        // Inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.data_type, false));
        }
    }
    else {
        LOG(FATAL) << "Invalid function kind: " << func_spec.kind;
    }

    return output;
}

}  // namespace

absl::StatusOr<data::GraphNode> AddElemsOp_CreateNode(GraphOpsContext& ctx, const NodeFunctionSpec& fnspec) {
    const GraphConfig& config = ctx.config;
    GraphState& state = ctx.state;
    auto* const toposort_order = ctx.toposort_order;

    const uint32_t raw_id = ++(state.idgen_state.next_node_id);
    const std::string alphanum_id = GenSplitMix64OfLength(raw_id + config.nodeid_splitmix_offset, 10);

    std::vector<data::GraphSlot> new_slots = CreateNodeSlotsV2(fnspec, raw_id, alphanum_id);

    std::vector<std::string> slot_names;
    slot_names.reserve(new_slots.size());
    for (const auto& slot : new_slots) {
        slot_names.push_back(slot.name);
        state.slots[std::make_tuple(slot.parent, slot.name)] = std::move(slot);
    }

    data::GraphNode graph_node = {
        .id = raw_id,
        .alnumid = alphanum_id,
        .fnuri = "/fn/points-on-curve",
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
