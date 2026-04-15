#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/IdTypes.h"

// plstate: Pipeline satte
// Varying state information for pipeline elements, node, slot etc.

namespace ujcore::plstate {

struct SlotState {
    // TODO: Store data about:
    // Manual  entry.

    // Edge connections: Raw ids of edges.
    std::set<EdgeId> inEdges;
    std::set<EdgeId> outEdges;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_STRUCT_VISITOR_FUNCTION(inEdges, outEdges, genId);
};

struct NodeState {
    enum class ConnectedState {
        WAIT,
        RUN,
        ERR,
    };

    // UI visible label.
    // TODO: Enable u8 string in JS conversion. Use std::u8string
    std::string label;

    ConnectedState connected = ConnectedState::WAIT;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_ENUM_CONVERSION_FUNCTION(ConnectedState, WAIT, RUN, ERR);
    DEFINE_STRUCT_VISITOR_FUNCTION(label, connected, genId);
};

}  // namespace ujcore::plstate
