#pragma once

#include <cstdint>
#include <string>

#include "cppschema/common/visitor_macros.h"

namespace ujcore::data {

struct GraphSlot {
    // Id (raw) of the parent node.
    uint32_t parent;

    // The slot name. E.g. "in:x", "out:fx"
    std::string name;

    // The data type of what flows through the slot.
    std::string dtype;

    // Is it a output slot, or an input slot.
    bool isoutput {false};

    // Last modified timestamp or generation id.
    // TODO: Support uint64_t <-> bigint (JS) before exposing this field.
    int64_t modified {0LL};

    DEFINE_STRUCT_VISITOR_FUNCTION(parent, name, dtype, isoutput);
};

using SlotId = std::tuple<uint32_t /* parent node id */, std::string /* slot name */>;

}  // namespace ujcore::data
