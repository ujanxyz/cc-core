#pragma once

#include <cstdint>
#include <string>

namespace buildinfo {

struct BuildInfo {
    int32_t timestamp = 0;
    std::string hostname;
    std::string user;
    std::string revision;
    std::string status;
};

/**
 * Standalone global function to get the system build info.
 */ 
const BuildInfo& GetSystemBuildInfo();

class SystemBuildInfo {
  public:
    // Access the singleton instance.
    static SystemBuildInfo& Instance();

    // Returns the stored info.
    const BuildInfo& GetInfo() const;

    void set_timestamp(int32_t timestamp);
    void set_hostname(const std::string& hostname);
    void set_user(const std::string& user);
    void set_revision(const std::string& revision);
    void set_status(const std::string& status);

  private:
    BuildInfo info_;
};

}  // namespace buildinfo
