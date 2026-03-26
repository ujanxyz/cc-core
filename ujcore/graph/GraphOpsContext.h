#pragma once

#include "ujcore/graph/GraphState.hpp"
#include "ujcore/graph/TopoSortOrder.h"

#include <cstdint>
#include <memory>

namespace ujcore {

struct GraphOpsContext {
  // The config and state must always exist and outlive the context.
  const GraphConfig& config;
  GraphState& state;

  TopoSortOrder* const toposort_order;
};

}  // namespace ujcore
