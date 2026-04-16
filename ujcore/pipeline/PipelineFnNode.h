#pragma once

#include <map>
#include <memory>
#include <string_view>

#include "absl/status/statusor.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/FuncParamAccess.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionSpec.h"

namespace ujcore {

struct SlotStorage {
    FuncParamAccess access = FuncParamAccess::kUnknown;

    AttributeData attribute;

    // If true, this is a manually entered attribute, as opposed to one
    // coming through an incoming edge.
    // Manual attributes are prepared beforehand and only read execution,
    // they should not be cleared when the node is re-run.
    bool overridden = false;

    // Tracks if the slot was accessed during the node execution.
    // Used to track progressive execution.
    bool accessed = false;
};

class PipelineFnNode : public FunctionContextParent {
public:
  explicit PipelineFnNode(
      NodeId nodeId,
      const FunctionSpec& funcSpec,
      std::unique_ptr<FunctionBase> funcInstance);

  bool Init();

  // TODO: Return a RunResult struct.
  absl::StatusOr<bool> RunFunction();
  
  // Methods implementing `FunctionContextParent`.

  AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) override;
  void LogFromFunc(std::string_view message) override;
  void DumpDebugInfoFromFunc() override;

private:
    const NodeId selfId_;
    std::unique_ptr<FunctionBase> funcInstance_;
    std::unique_ptr<FunctionContext> functionCtx_;
    std::map<std::string /* slot name */, SlotStorage> slots_;

    bool executed_ = false;

    friend class PipelineRunner;
};

}  // namespace ujcore
