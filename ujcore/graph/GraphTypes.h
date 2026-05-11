#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/graph/IdTypes.h"

// GraphTypes: Merged types for pipeline info and state.
// Fixed information and varying state for pipeline elements: node, edge, slot etc.

namespace ujcore::grph {

// ============================================================================
// Info types (from plinfo.h)
// ============================================================================

struct SlotInfo {
    // Input params are read-only, output params are created from scratch.
    // Mutable params are both read and modified.
    // Note: This is copied from function info.
    /// @json: Encode as string, possible values: "I" | "O" | "M"
    enum class AccessEnum {
        I,  // Input
        O,  // Output
        M,  // Mutatable
    };
  
    // Id (raw) of the parent node.
    /// @json: Encode as number, like `21`
    NodeId parent;

    // The slot name. E.g. "x", "fx"
    /// @json: Encode as string, like `"x"`
    std::string name;

    // The data type of what flows through the slot.
    /// @json: Encode as string, like `"float"`
    std::string dtype;

    /// @json: Encode as enum string, see enum definition.
    AccessEnum access = AccessEnum::I;

    DEFINE_ENUM_CONVERSION_FUNCTION(AccessEnum, I, O, M);
    DEFINE_STRUCT_VISITOR_FUNCTION(parent, name, dtype, access);
};

struct NodeInfo {
    /// @json: Encode as string, possible values: "FN" | "IN" | "OUT"
    enum class NodeType {
        FN,  // Function
        IN,  // Graph input
        OUT,  // Graph output
    };

    // Valid node id should start from 1, i.e. always > 0.
    /// @json: Encode as number, like `21`
    NodeId rawId {0};

    // Base-62 alphanumeric id, e.g. "ZBqg1rBrgq". This is directly translated from the int64 id.
    /// @json: Encode as string, like `"ZBqg1rBrgq"`
    std::string alnumid;

    // The type of node, a function, or an input or output to the overall graph.
    /// @json: Encode as enum string, see enum definition.
    NodeType ntype = NodeType::FN;

    // Identifies the node function used, e.g. "/fn/points-on-curve"
    // The uri of graph inputs functions are named as: "/$IN/<dtype-as-str>"
    // The uri of graph outputs functions are named as: "/$OUT/<dtype-as-str>"
    /// @json: Encode as string, like `"/fn/points-on-curve"`
    std::string uri;

    // Only for graph IO nodes, the data type of the graph input or output.
    /// @json: Encode as string (if exists), like `"float"` or missing for non-IO nodes.
    std::optional<std::string> ioDtype;

    // names of the slots contained in this node.
    /// @json: Encode as list of strings, like `["x", "y", "fx"]`
    std::vector<std::string> ins;
    /// @json: Encode as list of strings, like `["out"]`
    std::vector<std::string> outs;
    /// @json: Encode as list of strings, like `["inout1", "inout2"]`
    std::vector<std::string> inouts;

    DEFINE_ENUM_CONVERSION_FUNCTION(NodeType, FN, IN, OUT);
    DEFINE_STRUCT_VISITOR_FUNCTION(rawId, alnumid, ntype, uri, ioDtype, ins, outs, inouts);
};

struct EdgeInfo {
    // Valid node id should start from 1, i.e. always id > 0.
    /// @json: Encode as number, like `21`
    EdgeId id {0};

    // Concatenated id from parts.
    /// @json: Encode as string, like `"21|5|fx|42|in"`
    std::string catid;

    // Id (raw) of the source and target node.
    /// @json: Encode as number, like `21`
    NodeId node0 {0};
    /// @json: Encode as number, like `42`
    NodeId node1 {0};

    /// @json: Encode as string, like `"fx"` or `"in"`
    std::string slot0;
    /// @json: Encode as string, like `"fx"` or `"in"`
    std::string slot1;

    DEFINE_STRUCT_VISITOR_FUNCTION(id, catid, node0, node1, slot0, slot1);
};

// ============================================================================
// State types (from plstate.h)
// ============================================================================

struct EncodedData {
    // Encoded payload data.
    std::string payload;

    DEFINE_STRUCT_VISITOR_FUNCTION(payload);
};

struct SlotState {
    enum class Validity {
        VALID = 0,
        WARN_DATA,
        ERR_TYPE,
        ERR_EDGE,
    };

    // Edge connections: Raw ids of edges.
    std::set<EdgeId> inEdges;
    std::set<EdgeId> outEdges;

    // Manual override data.
    // Applies only to input and inout slot.
    std::optional<EncodedData> encodedData;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_ENUM_CONVERSION_FUNCTION(Validity, VALID, WARN_DATA, ERR_TYPE, ERR_EDGE);
    DEFINE_STRUCT_VISITOR_FUNCTION(inEdges, outEdges, encodedData, genId);
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
    std::optional<EncodedData> encodedData;

    ConnectedState connected = ConnectedState::WAIT;

    // Generation id to keep track of changes.
    int32_t genId {0LL};

    DEFINE_ENUM_CONVERSION_FUNCTION(ConnectedState, WAIT, RUN, ERR);
    DEFINE_STRUCT_VISITOR_FUNCTION(label, encodedData, connected, genId);
};

struct GraphRunOutput {
    NodeId nodeId;
    std::string dtype;
    std::optional<EncodedData> encodedData;

    DEFINE_STRUCT_VISITOR_FUNCTION(nodeId, dtype, encodedData);
};

}  // namespace ujcore::grph
