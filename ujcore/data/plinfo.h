#pragma once

#include <cstdint>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/IdTypes.h"

// plinfo: Pipeline info
// Fixed information for pipeline elements, node, edge, slot etc.

namespace ujcore::plinfo {

struct SlotInfo {
    // Input params are read-only, output params are created from scratch.
    // Mutable params are both read and modified.
    // Note: This is copied from function info.
    enum class AccessEnum {
        I,  // Input
        O,  // Output
        M,  // Mutatable
    };
  
    // Id (raw) of the parent node.
    NodeId parent;

    // The slot name. E.g. "x", "fx"
    std::string name;

    // The data type of what flows through the slot.
    std::string dtype;

    AccessEnum access = AccessEnum::I;

    DEFINE_ENUM_CONVERSION_FUNCTION(AccessEnum, I, O, M);
    DEFINE_STRUCT_VISITOR_FUNCTION(parent, name, dtype, access);
};

struct NodeInfo {
    // Valid node id should start from 1, i.e. always > 0.
    NodeId rawId {0};

    // Base-62 alphanumeric id, e.g. "ZBqg1rBrgq". This is directly translated from the int64 id.
    std::string alnumid;

    // Identifies the node function used, e.g. "/fn/points-on-curve"
    std::string fnuri;

    // names of the slots contained in this node.
    std::vector<std::string> ins;
    std::vector<std::string> outs;
    std::vector<std::string> inouts;

    DEFINE_STRUCT_VISITOR_FUNCTION(rawId, alnumid, fnuri, ins, outs, inouts);
};

struct EdgeInfo {
    // Valid node id should start from 1, i.e. always id > 0.
    EdgeId id {0};

    // Concatenated id from parts.
    std::string catid;

    // Id (raw) of the source and target node.
    NodeId node0 {0};
    NodeId node1 {0};

    std::string slot0;
    std::string slot1;

    DEFINE_STRUCT_VISITOR_FUNCTION(id, catid, node0, node1, slot0, slot1);
};

}  // namespace ujcore::plinfo
