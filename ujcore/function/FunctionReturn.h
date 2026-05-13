#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

#include "absl/status/status.h"

namespace ujfunc {

enum class ReturnCode : uint8_t {
  DONE = 0,
  NO_DATA,
  AWAIT,
  ERROR
};

// Await metadata
struct AwaitInfo {
  std::string channel;   // e.g. "wgpu"
  std::string token;     // unique work id
};

// Missing input metadata
struct NoDataInfo {
  std::vector<std::string> missingSlots;
};

// Final return struct
struct FunctionReturn {
  ReturnCode code {ReturnCode::DONE};

  // Optional payloads (only one is active depending on code)
  std::optional<AwaitInfo> await;
  std::optional<NoDataInfo> noData;
  // ABSL status is used for error (code == ERROR) to leverage its rich ecosystem,
  // Must not use absl::okStatus(). For the special scenarios like NO_DATA and AWAIT,
  // do not use ERROR code, instead use the dedicated codes, otherwise the JS side
  // will not be able to distinguish them from regular errors.
  std::optional<absl::Status> error;  // only for ERROR

  // Lookup functions.
  bool IsDone()   const { return code == ReturnCode::DONE; }
  bool IsAwait()  const { return code == ReturnCode::AWAIT; }
  bool IsError()  const { return code == ReturnCode::ERROR; }
  bool IsNoData() const { return code == ReturnCode::NO_DATA; }
};

} // namespace ujfunc
