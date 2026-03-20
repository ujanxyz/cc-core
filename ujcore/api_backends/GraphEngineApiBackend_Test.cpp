
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

    auto stats = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphEngineApi::ElementStats>("getElemStats", kVoid);
    EXPECT_EQ(stats.num_nodes, 0);

    std::string node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", "spec..");
    EXPECT_EQ(node_id, "pFE8FGqNWL");
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", "spec..");
    EXPECT_EQ(node_id, "s2GhcWpBLP");
    node_id = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, std::string>("addNode", "spec..");
    EXPECT_EQ(node_id, "ZBqg1rBrgq");

    stats = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphEngineApi::ElementStats>("getElemStats", kVoid);
    EXPECT_EQ(stats.num_nodes, 3);
}

}  // namespace
}  // namespace ujcore