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
#include "ujcore/pipeline/PipelineSlot.h"

namespace ujcore {

class PipelineFnNode : public FunctionContextParent {
public:
  explicit PipelineFnNode(
      NodeId nodeId,
      const FunctionSpec& funcSpec,
      std::unique_ptr<FunctionBase> funcInstance);

  PipelineSlot* LookupSlot(const std::string& slotName);

  // Execute the function stage in the pipeline.
  absl::StatusOr<bool> RunFunction();
  
  // Methods implementing `FunctionContextParent`.

  NodeId GetFunctiontNodeId() const override;
  AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) override;
  void LogFromFunc(std::string_view message) override;
  void DumpDebugInfoFromFunc() override;

  ResourceContext* GetResourceContext() override;

private:
    const NodeId selfId_;
    std::unique_ptr<FunctionBase> funcInstance_;
    std::unique_ptr<FunctionContext> functionCtx_;
    std::unique_ptr<ResourceContext> resourceCtx_;
    std::map<std::string /* slot name */, PipelineSlot> slots_;

    bool executed_ = false;

    friend class PipelineBuilder;
};

}  // namespace ujcore
