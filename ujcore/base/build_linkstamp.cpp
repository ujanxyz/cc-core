#include <string>

#include "ujcore/base/build_info.hpp"

// The code in this file has no public declaration.
// So everything is wrapped inside an anonymous namespace.
namespace {

// These variables are automatically defined by Bazel when 
// using the linkstamp attribute.

const int32_t build_timestamp = BUILD_TIMESTAMP;
const char build_host[] = BUILD_HOST;
const char build_user[] = BUILD_USER;
const char build_revision[] = BUILD_SCM_REVISION;
const char build_status[] = BUILD_SCM_STATUS;

static void __attribute__((constructor)) PopulateBuildInfoFields() {
    auto& sys_info = buildinfo::SystemBuildInfo::Instance();
    sys_info.set_timestamp(build_timestamp);
    sys_info.set_hostname(build_host);
    sys_info.set_user(build_user);
    sys_info.set_revision(build_revision);
    sys_info.set_status(build_status);
}

}  // namespace
