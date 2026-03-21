#include "ujcore/graph/ops/InsertNodeOp.hpp"

#include <vector>
#include <any>
#include <optional>

#include "absl/log/log.h"
#include "nlohmann/json.hpp"
#include "ujcore/functions/FunctionIoUtils.hpp"
#include "ujcore/graph/IdGenerator.hpp"

namespace ujcore {
namespace {

using json = ::nlohmann::json;

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

void CreateNodeSlots(const NodeFunctionSpec& func_spec, std::string_view node_id,
        std::vector<SlotData>& output) {
    output.clear();
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
}

std::string InsertNodeOp::apply(std::string payload_str, GraphState& state) {
    json j = json::parse(payload_str);
    if (!j.contains("spec") || !j["spec"].is_object()) {
        j = {};
        j["status"] = "JSON_ERROR";
        return j.dump();
    }

    NodeFunctionSpec spec;
    if (!ParseNodeFuncSpecFromJsonObj(j["spec"], spec)) {
        j = {};
        j["status"] = "PARSE_ERROR";
        return j.dump();
    }

    // TODO: Add logic to insert a graph node.
    // It should just mutate the graph state.
    const std::string new_nodeid = GenSplitMix64AndAdvance(state.idgen_state.next_node_id, 10);

    std::vector<SlotData> new_slots;
    CreateNodeSlots(spec, new_nodeid, new_slots);

    std::vector<std::string> slot_ids;
    slot_ids.reserve(new_slots.size());
    for (const auto& slot : new_slots) {
        slot_ids.push_back(slot.id);
        state.slots[slot.id] = std::move(slot);
    }

    LOG(INFO) << "InsertNodeOperator @ apply ... " << new_nodeid << "; spec: " << NodeFuncSpecToJsonStr(spec);
    NodeData node_data = {
        .func_uri = "/fn/points-on-curve",
        .label = u8"Point on Curve (2)",
        .spec = std::move(spec),
        .slot_ids = slot_ids,
    };
    state.nodes[new_nodeid] = node_data;

    j = {};
    j["nodeid"] = new_nodeid;
    return j.dump();
}

}  // namespace ujcore
