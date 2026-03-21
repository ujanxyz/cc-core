#pragma once

#include "ujcore/graph/GraphState.hpp"

namespace ujcore {

class GraphOperatorBase {
 public:
  virtual ~GraphOperatorBase() = default;

  virtual std::string apply(std::string payload_str, GraphState& state) = 0;
};

}  // namespace ujcore
