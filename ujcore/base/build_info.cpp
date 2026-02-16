#include "ujcore/base/build_info.hpp"

namespace buildinfo {
namespace {

// Default initialized field values.
void MakeDefaultInfo(BuildInfo& info) {
  info.timestamp = 0;
  info.hostname = "not set";
}

}  // namespace

/* static */
SystemBuildInfo& SystemBuildInfo::Instance() {
  static SystemBuildInfo* kInstance = ([]{
    SystemBuildInfo* singleton_info = new SystemBuildInfo();
    MakeDefaultInfo(singleton_info->info_);
    return singleton_info;
  })();
  return *kInstance;
}

const BuildInfo& SystemBuildInfo::GetInfo() const {
  return info_;
}

void SystemBuildInfo::set_timestamp(int32_t timestamp) {
  info_.timestamp = timestamp;
}

void SystemBuildInfo::set_hostname(const std::string& hostname) {
  info_.hostname = hostname;
}

void SystemBuildInfo::set_user(const std::string& user) {
  info_.user = user;
}

void SystemBuildInfo::set_revision(const std::string& revision) {
  info_.revision = revision;
}

void SystemBuildInfo::set_status(const std::string& status) {
  info_.status = status;
}

// The global (system wide) BuildInfo accessor API.
const BuildInfo& GetSystemBuildInfo() {
  return SystemBuildInfo::Instance().GetInfo();
}

}  // namespace buildinfo