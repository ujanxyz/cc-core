#pragma once

#include <string>
#include <tuple>

#include "ujcore/graph/GraphState.hpp"
#include "ujcore/graph/IdGenerator.hpp"

namespace ujcore {

class GraphEngine {
 public:
  GraphEngine();

  std::string InsertNewNode(const std::string& fn_spec);

  std::tuple<int32_t, int32_t, int32_t>
  GetElementCounts() const;

 private:
  GraphState state_;
  IdGenerator id_gen_;
};

}  // namespace ujcore
