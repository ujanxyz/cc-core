#pragma once

#include "ujcore/graph/GraphState.hpp"

#include <cstdint>
#include <string>

namespace ujcore {

class GraphOperatorBase {
 public:
  virtual ~GraphOperatorBase() = default;

  virtual void apply(GraphState& state) = 0;
};


class InsertNodeOperator : public GraphOperatorBase {
    struct Request {};

    void apply(GraphState& state) override;
}

}  // namespace ujcore
