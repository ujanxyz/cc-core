#pragma once

#include <set>
#include <string>

#include "ujcore/graph/GraphOpsContext.h"

namespace ujcore {

struct DeleteElementsOpMutableIds {
  std::set<std::string> node_ids;
  std::set<std::string> edge_ids;
};

void DoDeleteElemsOp(GraphOpsContext& ctx, DeleteElementsOpMutableIds& elem_ids);

}  // namespace ujcore