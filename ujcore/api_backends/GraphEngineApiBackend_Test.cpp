// bazel test //ujcore/api_backends:GraphEngineApiBackend_Test

#include "cppschema/apispec/api_registry.h"

#include "cppschema/common/types.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/api_schemas/GraphEngineApi.hpp"

namespace ujcore {
namespace {

using ::cppschema::ApiRegistry;
 
TEST(GraphEngineApiBackendTest, Basic) {
    VoidType kVoid;

    std::string payload = R"({
        "spec": {
            "kind": "lam",
            "label": "Noise",
            "description": "Generates noise",
            "data_type": "float",
            "arg_types": ["vec2"],
            "inputs": [
                { "name": "seed", "data_type": "int" }
            ]
        }
    })";

    auto stats = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphEngineApi::ElementStats>("getElemStats", kVoid);
    EXPECT_EQ(stats.num_nodes, 0);

    std::string node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addElems", payload);
    EXPECT_EQ(node_id, "{\"nodesAdded\":[\"pFE8FGqNWL\"]}");
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addElems", payload);
    EXPECT_EQ(node_id, "{\"nodesAdded\":[\"s2GhcWpBLP\"]}");


    payload = R"({
        "edges": [
          {
            "srcNode": "pFE8FGqNWL",
            "destNode": "s2GhcWpBLP",
            "srcSlot": "aaa",
            "destSlot": "bbb"
          }
        ]
    })";
    payload = R"({
        "edges": [
          {
            "srcNode": "s2GhcWpBLP",
            "destNode": "pFE8FGqNWL",
            "srcSlot": "aaa",
            "destSlot": "bbb"
          }
        ]
    })";
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addElems", payload);
    EXPECT_EQ(node_id, "mmmm");

    // stats = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphEngineApi::ElementStats>("getElemStats", kVoid);
    // EXPECT_EQ(stats.num_nodes, 3);
    // EXPECT_EQ(stats.num_slots, 6);

    payload = R"({
        "nodes": ["ZBqg1rBrgq"],
        "edges": []
    })";
    std::string deleteRes = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("deleteElems", payload);
    EXPECT_EQ(deleteRes, "{\"nodesDeleted\":[\"ZBqg1rBrgq\"]}");
}

}  // namespace
}  // namespace ujcore