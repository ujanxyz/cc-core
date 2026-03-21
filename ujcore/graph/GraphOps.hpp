#pragma once

#include "ujcore/graph/ops/GraphOperatorBase.hpp"

#include <cstdint>
#include <memory>

namespace ujcore {

struct GraphOps {
    std::unique_ptr<GraphOperatorBase> insert_op;
    std::unique_ptr<GraphOperatorBase> delete_elems_op;
};

void InitializeGraphOps(GraphOps& ops);

}  // namespace ujcore
