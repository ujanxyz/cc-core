
#include "cppschema/apispec/api_registry.h"

#include "cppschema/common/types.h"
#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "ujcore/api_schemas/PipelineApi.hpp"

namespace ujcore {
namespace {

using ::cppschema::ApiRegistry;
 
TEST(PipelineApiBackendTest, Basic) {
    VoidType kVoid;
    const int ret = ApiRegistry<PipelineApi>::Get().template Call<VoidType, int>("clearGraph", kVoid);
    EXPECT_EQ(ret, 11);
}

}  // namespace
}  // namespace ujcore