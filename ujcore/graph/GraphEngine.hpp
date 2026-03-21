#pragma once

#include <string>
#include <memory>
#include <tuple>

#include "ujcore/graph/GraphState.hpp"

namespace ujcore {

class GraphEngine {
 public:
  GraphEngine();

  GraphState* mutable_state() {
    return &state_;
  }

  std::tuple<int32_t, int32_t, int32_t>
  GetElementCounts() const;

 private:
  GraphState state_;
};

}  // namespace ujcore
