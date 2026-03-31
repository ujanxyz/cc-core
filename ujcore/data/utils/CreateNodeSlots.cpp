#include "ujcore/data/utils/CreateNodeSlots.h"

#include <string>

#include "absl/status/status.h"

namespace ujcore::data {

GraphSlot MakeSlotV2(const uint32_t parent, const std::string& name, const std::string& dtype, const bool isoutput) {
    return GraphSlot {
        .parent = parent,
        .name = name,
        .dtype = dtype,
        .isoutput = isoutput,
    };
}

absl::StatusOr<std::vector<GraphSlot>>
CreateNodeSlots(const FunctionInfo& fn_info, const uint32_t node_id, const std::string& node_alnumid) {
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


    std::vector<GraphSlot> output;

    const FnExtendedInfo& ext = fn_info.ext;

    if (ext.input.has_value()) {
        const auto& input = *ext.input;
        output.push_back(
            MakeSlotV2(node_id, out_id(), input.dtype, true));
    }
    else if (ext.output.has_value()) {
        const auto& output_xxxxxxxx = *ext.output;
        output.push_back(
            MakeSlotV2(node_id, in_id(), output_xxxxxxxx.dtype, false));
    }
    else if (ext.purefn.has_value()) {
        const auto& purefn = *ext.purefn;
        // Function inputs
        for (const auto& p : purefn.ins) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.dtype, false));
        }
        // Function outputs
        for (const auto& p : purefn.outs) {
            output.push_back(
                MakeSlotV2(node_id, out_id(p.name), p.dtype, true));
        }
    }
    else if (ext.opfn.has_value()) {
        const auto& opfn = *ext.opfn;
        // Implicit input/output
        output.push_back(
            MakeSlotV2(node_id, in_id(), opfn.dtype, false));
        output.push_back(
            MakeSlotV2(node_id, out_id(), opfn.dtype, true));

        // Named inputs
        for (const auto& p : opfn.ins) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.dtype, false));
        }
    }
    else if (ext.lambda.has_value()) {
        const auto& lambda = *ext.lambda;
        // Output (functor)
        output.push_back(
            MakeSlotV2(node_id, out_id(), lambda.dtype, true));

        for (const auto& p : lambda.ins) {
            output.push_back(
                MakeSlotV2(node_id, in_id(p.name), p.dtype, false));
        }
    }
    else {
        return absl::InvalidArgumentError("Missing func extension");
    }

    return output;
}

}  // namespace ujcore::data
