// bazel test //ujcore/api_backends:GraphApiBackend_Test

#include "cppschema/apispec/api_registry.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cppschema/common/types.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/api_schemas/GraphApi.h"
#include "ujcore/graph/AbslStringifies.h"
#include "ujcore/graph/FunctionInfo.h"
#include "ujcore/utils/IdUtils.h"

namespace ujcore {
namespace {

using ::cppschema::ApiRegistry;
using ::testing::ElementsAre;
using ::ujcore::FunctionInfo;

using GetGraphResponse = GraphApi::GetGraphResponse;
using CreateNodeRequest = GraphApi::CreateNodeRequest;
using CreateNodeResponse = GraphApi::CreateNodeResponse;
using AddEdgeRequest = GraphApi::AddEdgeRequest;
using AddEdgeResponse = GraphApi::AddEdgeResponse;
using DeleteElementsRequest = GraphApi::DeleteElementsRequest;
using DeleteElementsResponse = GraphApi::DeleteElementsResponse;

TEST(GraphApiBackendTest, Basic) {
    VoidType kVoid;
    const std::string node1_alnumid = EncodeStringId(NodeId(1));
    const std::string node2_alnumid = EncodeStringId(NodeId(2));
    CreateNodeRequest create_req = {
        .func = FunctionInfo {
            .uri = "/testing/displace-point",
            .label = "Displace point",
            .desc = "[Testing] Displace a 2D point along X-axis by a given delta",
        },
    };
    
    CreateNodeResponse create_resp = ApiRegistry<GraphApi>::Get().template Call<CreateNodeRequest, CreateNodeResponse>("createNode", create_req);
    ASSERT_TRUE(create_resp.nodeInfo.has_value());
    EXPECT_EQ(
        absl::StrCat(*create_resp.nodeInfo),
        absl::StrCat("(n#1:", node1_alnumid, "; fn:/testing/displace-point; ins:p,dx; outs:fp)"));

    create_resp = ApiRegistry<GraphApi>::Get().template Call<CreateNodeRequest, CreateNodeResponse>("createNode", create_req);
    ASSERT_TRUE(create_resp.nodeInfo.has_value());
    EXPECT_EQ(
        absl::StrCat(*create_resp.nodeInfo),
        absl::StrCat("(n#2:", node2_alnumid, "; fn:/testing/displace-point; ins:p,dx; outs:fp)"));

    AddEdgeRequest add_edge_req1 = {
        .sourceNode = NodeId(2),
        .sourceSlot = "fp",
        .targetNode = NodeId(1),
        .targetSlot = "p",
    };
    AddEdgeRequest add_edge_req2 = {
        .sourceNode = NodeId(1),
        .sourceSlot = "fp",
        .targetNode = NodeId(2),
        .targetSlot = "p",
    };

    AddEdgeResponse edge_resp1 = ApiRegistry<GraphApi>::Get().template Call<AddEdgeRequest, AddEdgeResponse>("addEdge", add_edge_req1);
    ASSERT_TRUE(edge_resp1.edgeInfo.has_value());
    EXPECT_EQ(
        absl::StrCat(*edge_resp1.edgeInfo),
        absl::StrCat("(", node2_alnumid, "$fp--", node1_alnumid, "$p: [2/fp] -> [1/p])"));

    AddEdgeResponse edge_resp2 = ApiRegistry<GraphApi>::Get().template Call<AddEdgeRequest, AddEdgeResponse>("addEdge", add_edge_req2);
    ASSERT_TRUE(edge_resp2.edgeInfo.has_value());
    EXPECT_EQ(
        absl::StrCat(*edge_resp2.edgeInfo),
        absl::StrCat("(", node1_alnumid, "$fp--", node2_alnumid, "$p: [1/fp] -> [2/p])"));


    GetGraphResponse graph = ApiRegistry<GraphApi>::Get().template Call<VoidType, GetGraphResponse>("getGraph", kVoid);
    ASSERT_EQ(graph.nodeInfos.size(), 2);
    ASSERT_EQ(graph.edgeInfos.size(), 2);
    ASSERT_EQ(graph.slotInfos.size(), 6);
    LOG(INFO) << "Graph nodes: " << absl::StrCat(graph.nodeInfos);
    LOG(INFO) << "Graph edges: " << absl::StrCat(graph.edgeInfos);
    LOG(INFO) << "Graph slots: " << absl::StrCat(graph.slotInfos);


    DeleteElementsRequest deleteReq1 = {
        .nodeIds = {NodeId(1)},
        .edgeIds = {},
    };
    DeleteElementsResponse deleteResp = ApiRegistry<GraphApi>::Get().template Call<DeleteElementsRequest, DeleteElementsResponse>("deleteElements", deleteReq1);
    EXPECT_THAT(deleteResp.nodeIds, ElementsAre(NodeId(1)));
    EXPECT_THAT(deleteResp.edgeIds, ElementsAre());

}

}  // namespace
}  // namespace ujcore