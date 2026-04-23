#pragma once

#include "ujcore/function/AttributeData.h"
#include "ujcore/function/FuncParamAccess.h"

namespace ujcore {

struct PipelineSlot {
    FuncParamAccess access = FuncParamAccess::kUnknown;

    AttributeData attribute;

    // If true, this is a manually entered attribute, as opposed to one
    // coming through an incoming edge.
    // Manual attributes are prepared beforehand and only read execution,
    // they should not be cleared when the node is re-run.
    bool overridden = false;

    // Tracks if the slot was accessed during the node execution.
    // Used to track progressive execution.
    // TODO: Re-visit the necessity and correctness of this field.
    bool accessed = false;
};

}  // namespace ujcore
