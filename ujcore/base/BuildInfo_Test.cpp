#include "ujcore/base/BuildInfo.h"

#include "gtest/gtest.h"

namespace buildinfo {
    
TEST(BuildInfoTest, Basic) {
    const BuildInfo& build_info = GetSystemBuildInfo();
    EXPECT_EQ(build_info.timestamp, 0);
    EXPECT_EQ(build_info.hostname, "redacted");
    EXPECT_EQ(build_info.user, "redacted");
    EXPECT_EQ(build_info.revision, "0");
    EXPECT_EQ(build_info.status, "redacted");
}

}  // namespace buildinfo
