
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

    const std::string payload = R"({
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

    std::string node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", payload);
    EXPECT_EQ(node_id, "{\"nodeid\":\"pFE8FGqNWL\"}");
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", payload);
    EXPECT_EQ(node_id, "{\"nodeid\":\"s2GhcWpBLP\"}");
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", payload);
    EXPECT_EQ(node_id, "{\"nodeid\":\"ZBqg1rBrgq\"}");

    stats = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphEngineApi::ElementStats>("getElemStats", kVoid);
    EXPECT_EQ(stats.num_nodes, 3);
    EXPECT_EQ(stats.num_slots, 6);
}

}  // namespace
}  // namespace ujcore