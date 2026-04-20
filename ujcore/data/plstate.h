#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/IdTypes.h"

// plstate: Pipeline state.
// Varying state information for pipeline elements, node, slot etc.

namespace ujcore::plstate {

struct EncodedData {
    // Encoded payload data.
    std::string payload;

    DEFINE_STRUCT_VISITOR_FUNCTION(payload);
};

struct SlotState {
    // Edge connections: Raw ids of edges.
    std::set<EdgeId> inEdges;
    std::set<EdgeId> outEdges;

    // Manual override data.
    // Applies only to input and inout slot.
    std::optional<EncodedData> manual;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_STRUCT_VISITOR_FUNCTION(inEdges, outEdges, manual, genId);
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

    // Only for graph IO nodes, this holds the data for the graph inputs and outputs.
    std::optional<EncodedData> ioData;

    ConnectedState connected = ConnectedState::WAIT;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_ENUM_CONVERSION_FUNCTION(ConnectedState, WAIT, RUN, ERR);
    DEFINE_STRUCT_VISITOR_FUNCTION(label, ioData, connected, genId);
};

struct GraphRunResult {
    // Encoded output data for each output node. Keyed by node raw id.
    std::map<NodeId, std::optional<std::string>> outputs;

    DEFINE_STRUCT_VISITOR_FUNCTION(outputs);
};

}  // namespace ujcore::plstate
