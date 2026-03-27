// bazel test //ujcore/api_backends:GraphEngineApiBackend_Test

#include "cppschema/apispec/api_registry.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cppschema/common/types.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/api_schemas/GraphEngineApi.hpp"
#include "ujcore/data/graph/AbslStringifies.h"

namespace ujcore {
namespace {

using ::cppschema::ApiRegistry;
using ::testing::ElementsAre;

using AddEdgesRequest = GraphEngineApi::AddEdgesRequest;
using AddEdgesResponse = GraphEngineApi::AddEdgesResponse;
using GraphDataResponse = GraphEngineApi::GraphDataResponse;
using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;


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
    
    CreateNodeResponse create_resp = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, CreateNodeResponse>("createNode", payload);
    ASSERT_TRUE(create_resp.node.has_value());
    EXPECT_EQ(absl::StrCat(*create_resp.node), "(n#1:s2GhcWpBLP, slots:$out,$in:seed, fn:/fn/points-on-curve)");

    create_resp = ApiRegistry<GraphEngineApi>::Get().template Call<std::string, CreateNodeResponse>("createNode", payload);
    ASSERT_TRUE(create_resp.node.has_value());
    EXPECT_EQ(absl::StrCat(*create_resp.node), "(n#2:ZBqg1rBrgq, slots:$out,$in:seed, fn:/fn/points-on-curve)");

    data::AddEdgeEntry add_edge_1 = {
        .node0 = "s2GhcWpBLP",
        .slot0 = "$out",
        .node1 = "ZBqg1rBrgq",
        .slot1 = "$in:seed",
    };
    data::AddEdgeEntry add_edge_2 = {
        .node0 = "ZBqg1rBrgq",
        .slot0 = "$out",
        .node1 = "s2GhcWpBLP",
        .slot1 = "$in:seed",
    };
    AddEdgesRequest add_edges_req = {
        .entries = { add_edge_1, add_edge_2 },
    };
    AddEdgesResponse edges_resp = ApiRegistry<GraphEngineApi>::Get().template Call<AddEdgesRequest, AddEdgesResponse>("addEdges", add_edges_req);
    ASSERT_EQ(edges_resp.edges.size(), 2);

    GraphDataResponse graph = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GraphDataResponse>("getGraph", kVoid);
    ASSERT_EQ(graph.nodes.size(), 2);
    ASSERT_EQ(graph.edges.size(), 2);
    ASSERT_EQ(graph.slots.size(), 4);
    // EXPECT_EQ(absl::StrCat(graph.slots), "{(out-slot#1 @ n#1, s2GhcWpBLP$out/float), (in-slot#2 @ n#1, s2GhcWpBLP$in:seed/int), (out-slot#3 @ n#2, ZBqg1rBrgq$out/float), (in-slot#4 @ n#2, ZBqg1rBrgq$in:seed/int)}");
    LOG(INFO) << "Graph nodes: " << absl::StrCat(graph.nodes);
    LOG(INFO) << "Graph edges: " << absl::StrCat(graph.edges);
    LOG(INFO) << "Graph slots: " << absl::StrCat(graph.slots);

    data::NodeAndEdgeIds node_n_edge_ids = {
        .node_ids = {1},
        .edge_ids = {},
    };
    node_n_edge_ids = ApiRegistry<GraphEngineApi>::Get().template Call<data::NodeAndEdgeIds, data::NodeAndEdgeIds>("deleteElements", node_n_edge_ids);
    ASSERT_THAT(node_n_edge_ids.node_ids, ElementsAre(1));
    ASSERT_THAT(node_n_edge_ids.edge_ids, ElementsAre());

}

}  // namespace
}  // namespace ujcore