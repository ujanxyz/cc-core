// bazel test //ujcore/api_backends:FlowApiBackend_Test

#include "cppschema/apispec/api_registry.h"

#include "gtest/gtest.h"
#include "ujcore/api_schemas/FlowApi.h"

namespace ujcore {
namespace {

TEST(FlowApiBackendTest, GetFlowStatusReturnsDummyStatus) {
    VoidType kVoid;
    const auto response = cppschema::ApiRegistry<FlowApi>::Get()
        .template Call<VoidType, FlowApi::GetFlowStatusResponse>("getFlowStatus", kVoid);

    EXPECT_EQ(response.status, "ready");
    EXPECT_TRUE(response.healthy);
}

}  // namespace
}  // namespace ujcore
