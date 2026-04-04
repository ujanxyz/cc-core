// bazel test //ujcore/api_backends:GraphEngineApiBackend_Test

#include "cppschema/apispec/api_registry.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cppschema/common/types.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/api_schemas/GraphEngineApi.h"
#include "ujcore/data/functions/FunctionInfo.h"
#include "ujcore/data/AbslStringifies.h"

namespace ujcore {
namespace {

using ::cppschema::ApiRegistry;
using ::testing::ElementsAre;
using ::ujcore::data::FunctionInfo;
using ::ujcore::data::ParamInfo;

using GetGraphResponse = GraphEngineApi::GetGraphResponse;
using CreateNodeRequest = GraphEngineApi::CreateNodeRequest;
using CreateNodeResponse = GraphEngineApi::CreateNodeResponse;
using AddEdgeRequest = GraphEngineApi::AddEdgeRequest;
using AddEdgeResponse = GraphEngineApi::AddEdgeResponse;
using DeleteElementsRequest = GraphEngineApi::DeleteElementsRequest;
using DeleteElementsResponse = GraphEngineApi::DeleteElementsResponse;

TEST(GraphEngineApiBackendTest, Basic) {
    VoidType kVoid;
    CreateNodeRequest create_req = {
        .func = FunctionInfo {
            .uri = "/fn/geom/translate-x",
            .label = "Translate Point X",
            .desc = "Translate a 2D point along X-axis by a given delta",
            .params = {
                ParamInfo { .name = "p", .dtype = "point2d", .access = ParamInfo::AccessEnum::I },
                ParamInfo { .name = "dx", .dtype = "float", .access = ParamInfo::AccessEnum::I },
                ParamInfo { .name = "fp", .dtype = "point2d", .access = ParamInfo::AccessEnum::O },
            },
        },
    };
    
    CreateNodeResponse create_resp = ApiRegistry<GraphEngineApi>::Get().template Call<CreateNodeRequest, CreateNodeResponse>("createNode", create_req);
    ASSERT_TRUE(create_resp.nodeInfo.has_value());
    EXPECT_EQ(absl::StrCat(*create_resp.nodeInfo), "(n#1:s2GhcWpBLP; fn:/fn/geom/translate-x; ins:p,dx; outs:fp)");

    create_resp = ApiRegistry<GraphEngineApi>::Get().template Call<CreateNodeRequest, CreateNodeResponse>("createNode", create_req);
    ASSERT_TRUE(create_resp.nodeInfo.has_value());
    EXPECT_EQ(absl::StrCat(*create_resp.nodeInfo), "(n#2:ZBqg1rBrgq; fn:/fn/geom/translate-x; ins:p,dx; outs:fp)");

    AddEdgeRequest add_edge_req1 = {
        .sourceNode = 2,
        .sourceSlot = "fp",
        .targetNode = 1,
        .targetSlot = "p",
    };
    AddEdgeRequest add_edge_req2 = {
        .sourceNode = 1,
        .sourceSlot = "fp",
        .targetNode = 2,
        .targetSlot = "p",
    };

    AddEdgeResponse edge_resp1 = ApiRegistry<GraphEngineApi>::Get().template Call<AddEdgeRequest, AddEdgeResponse>("addEdge", add_edge_req1);
    ASSERT_TRUE(edge_resp1.edgeInfo.has_value());
    EXPECT_EQ(absl::StrCat(*edge_resp1.edgeInfo), "(ZBqg1rBrgq$fp--s2GhcWpBLP$p: [2/fp] -> [1/p])");

    AddEdgeResponse edge_resp2 = ApiRegistry<GraphEngineApi>::Get().template Call<AddEdgeRequest, AddEdgeResponse>("addEdge", add_edge_req2);
    ASSERT_TRUE(edge_resp2.edgeInfo.has_value());
    EXPECT_EQ(absl::StrCat(*edge_resp2.edgeInfo), "(s2GhcWpBLP$fp--ZBqg1rBrgq$p: [1/fp] -> [2/p])");


    GetGraphResponse graph = ApiRegistry<GraphEngineApi>::Get().template Call<VoidType, GetGraphResponse>("getGraph", kVoid);
    ASSERT_EQ(graph.nodeInfos.size(), 2);
    ASSERT_EQ(graph.edgeInfos.size(), 2);
    ASSERT_EQ(graph.slotInfos.size(), 6);
    LOG(INFO) << "Graph nodes: " << absl::StrCat(graph.nodeInfos);
    LOG(INFO) << "Graph edges: " << absl::StrCat(graph.edgeInfos);
    LOG(INFO) << "Graph slots: " << absl::StrCat(graph.slotInfos);

    DeleteElementsRequest deleteReq1 = {
        .nodeIds = {1},
        .edgeIds = {},
    };
    DeleteElementsResponse deleteResp = ApiRegistry<GraphEngineApi>::Get().template Call<DeleteElementsRequest, DeleteElementsResponse>("deleteElements", deleteReq1);
    EXPECT_THAT(deleteResp.nodeIds, ElementsAre(1));
    EXPECT_THAT(deleteResp.edgeIds, ElementsAre());

}

}  // namespace
}  // namespace ujcore