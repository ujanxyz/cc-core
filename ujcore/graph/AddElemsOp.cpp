#include "ujcore/graph/AddElemsOp.h"

#include <vector>
#include <any>
#include <optional>

#include "absl/log/log.h"
#include "ujcore/graph/IdGenerator.h"

namespace ujcore {
namespace {

static SlotData MakeSlot(std::string_view id, std::string node_id, std::string dtype, bool is_output) {
    SlotData s;
    s.id = id;
    s.node_id = std::move(node_id);
    s.dtype = std::move(dtype);
    s.is_output = is_output;
    s.payload.reset();
    s.edge_ids.clear();
    s.modified = 0;
    return s;
}

}  // namespace

std::vector<SlotData> CreateNodeSlots(const NodeFunctionSpec& func_spec, std::string_view node_id, uint64_t& next_slotid) {
    std::vector<SlotData> output;
    const std::string nid(node_id);

    // Helper lambdas for naming
    auto in_id  = [&](std::string_view name = "") {
        return name.empty()
            ? nid + "$in"
            : nid + "$in:" + std::string(name);
    };

    auto out_id = [&](std::string_view name = "") {
        return name.empty()
            ? nid + "$out"
            : nid + "$out:" + std::string(name);
    };

    if (func_spec.kind == "in") {
        const auto& ext = std::any_cast<const InputNodeExt&>(func_spec.ext);

        output.push_back(
            MakeSlot(out_id(), nid, ext.data_type, true));
    }
    else if (func_spec.kind == "out") {
        const auto& ext = std::any_cast<const OutputNodeExt&>(func_spec.ext);

        output.push_back(
            MakeSlot(in_id(), nid, ext.data_type, false));
    }
    else if (func_spec.kind == "fn") {
        const auto& ext = std::any_cast<const FunctionNodeExt&>(func_spec.ext);

        // Inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlot(in_id(p.name), nid, p.data_type, false));
        }

        // Outputs
        for (const auto& p : ext.outputs) {
            output.push_back(
                MakeSlot(out_id(p.name), nid, p.data_type, true));
        }
    }
    else if (func_spec.kind == "op") {
        const auto& ext = std::any_cast<const OperatorNodeExt&>(func_spec.ext);

        // Implicit input/output
        output.push_back(
            MakeSlot(in_id(), nid, ext.data_type, false));

        output.push_back(
            MakeSlot(out_id(), nid, ext.data_type, true));

        // Named inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlot(in_id(p.name), nid, p.data_type, false));
        }
    }
    else if (func_spec.kind == "lam") {
        const auto& ext = std::any_cast<const LambdaNodeExt&>(func_spec.ext);

        // Output (functor)
        output.push_back(
            MakeSlot(out_id(), nid, ext.data_type, true));

        // Inputs
        for (const auto& p : ext.inputs) {
            output.push_back(
                MakeSlot(in_id(p.name), nid, p.data_type, false));
        }
    }
    else {
        LOG(FATAL) << "Invalid function kind: " << func_spec.kind;
    }
    return output;
}

void DoAddElemsOp(GraphOpsContext& ctx, const std::vector<NodeFunctionSpec>& func_specs, AddElemsResult& result) {
    const GraphConfig& config = ctx.config;
    GraphState& state = ctx.state;
    auto* const toposort_order = ctx.toposort_order;
    for (const NodeFunctionSpec& func_spec : func_specs) {
        const int64_t raw_id = (state.idgen_state.next_node_id)++;
        const std::string alphanum_nodeid = GenSplitMix64OfLength(raw_id + config.nodeid_splitmix_offset, 10);

        std::vector<SlotData> new_slots = CreateNodeSlots(func_spec, alphanum_nodeid, state.idgen_state.next_slot_id);

        std::vector<std::string> slot_ids;
        slot_ids.reserve(new_slots.size());
        for (const auto& slot : new_slots) {
            slot_ids.push_back(slot.id);
            state.slots[slot.id] = std::move(slot);
        }

        NodeData node_data = {
            .alnum_id = alphanum_nodeid,
            .func_uri = "/fn/points-on-curve",
            .label = u8"Point on Curve (2)",
            .spec = std::move(func_spec),
            .slot_ids = slot_ids,
        };
        state.nodes[alphanum_nodeid] = node_data;
        toposort_order->AddNode(alphanum_nodeid);
        result.nodes_added.insert(alphanum_nodeid);
    }
}

}  // namespace ujcore
