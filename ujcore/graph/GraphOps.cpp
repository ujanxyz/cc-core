#include "ujcore/graph/GraphOps.hpp"

#include "nlohmann/json.hpp"
#include "ujcore/functions/FunctionIoUtils.hpp"
#include "ujcore/graph/GraphState.hpp"
#include "ujcore/graph/IdGenerator.hpp"
#include "ujcore/graph/ops/InsertNodeOp.hpp"

#include <iostream>

namespace ujcore {
namespace {

using json = ::nlohmann::json;

class DeleteElemsOperator : public GraphOperatorBase {
    std::string apply(std::string payload_str, GraphState& state) override {
        // TODO: Add logic to delete elements in a graph (nodes and edges).
        std::cout << "DeleteElemsOperator @ apply ...\n";
        json j;
        j["status"] = "TODO";
        return j.dump();
    }
};

}  // namespace

void InitializeGraphOps(GraphOps& ops) {
    ops.insert_op = std::make_unique<InsertNodeOp>();
    ops.delete_elems_op = std::make_unique<DeleteElemsOperator>();
}

}  // namespace ujcore
