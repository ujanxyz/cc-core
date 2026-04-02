#pragma once

#include <cstdint>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"

// plstate: Pipeline satte
// Varying state information for pipeline elements, node, slot etc.

namespace ujcore::plstate {

struct SlotState {
    // Generation id to keep track of changes.
    int32_t gen {0LL};

    // TODO: Store data about:
    // Manual entry.
    // Edge connections.

    DEFINE_STRUCT_VISITOR_FUNCTION(gen);
};

struct NodeState {
    enum class ConnectedState {
        WAIT,
        RUN,
        ERR,
    };

    // Generation id to keep track of changes.
    int32_t gen {0LL};

    // UI visible label.
    // TODO: Enable u8 string in JS conversion. Use std::u8string
    std::string label;

    ConnectedState connected = ConnectedState::WAIT;

    DEFINE_ENUM_CONVERSION_FUNCTION(ConnectedState, WAIT, RUN, ERR);
    DEFINE_STRUCT_VISITOR_FUNCTION(gen, label, connected);
};

}  // namespace ujcore::plstate
