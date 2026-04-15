#pragma once

#include <vector>

#include "ujcore/data/IdTypes.h"

namespace ujcore {

struct TopoSortState {
    // The current topologically-sorted node order.
    std::vector<NodeId> sortOrder;

    // Is the order modified since last clear dirty bit call.
    bool hasDirtyBit = false;
};

}  // namespace ujcore
