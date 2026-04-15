#pragma once

#include <string>

#include "cppschema/common/strong_types.h"
#include "cppschema/common/visitor_macros.h"

namespace ujcore {

DEFINE_STRONG_UINT_TYPE(NodeId);
DEFINE_STRONG_UINT_TYPE(EdgeId);

struct SlotId {
    NodeId parent;  // parent node id
    std::string name;  // slot name

    DEFINE_STRUCT_VISITOR_FUNCTION(parent, name);
};

}  // namespace ujcore

template <>
struct std::less<ujcore::SlotId>
{
    bool operator()(const ujcore::SlotId &a, const ujcore::SlotId &b) const {
        if (a.parent != b.parent) {
            return a.parent < b.parent;
        } else {
            return a.name.compare(b.name) < 0;
        }
    }
};
