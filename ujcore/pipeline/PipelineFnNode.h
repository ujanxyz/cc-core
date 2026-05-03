#pragma once

#include <map>
#include <memory>
#include <string_view>

#include "absl/status/statusor.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
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

  NodeId GetFunctionNodeId() const override;
  AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) override;
  void LogFromFunc(std::string_view message) override;
  void DumpDebugInfoFromFunc() override;

  ResourceContext* GetResourceContext() override;

private:
public:

    // Holds data and references for a single slot.
    struct PerSlotEntry {
        // The data type specified in the slot info. This is used to decode the manually overridden
        // data for this slot, if exists.
        AttributeDataType dtype = AttributeDataType::kUnknown;

        // Directly references to the entry in graph's slot state. This allows the function
        // node to read the latest manually overridden data for this slot.
        const std::optional<plstate::EncodedData>* encodedInput = nullptr;

        // Only used for input slots, to convert manually overridden encoded data (if exists)
        // to internal attribute during execution.
        // This is populated for all slots except output slots.
        const AttributeDecodeFn* decodeFnPtr = nullptr;

        std::unique_ptr<PipelineSlot> plSlot;
    };

    const NodeId selfId_;
    std::unique_ptr<FunctionBase> funcInstance_;
    std::unique_ptr<FunctionContext> functionCtx_;
    std::unique_ptr<ResourceContext> resourceCtx_;
    std::map<std::string /* slot name */, PerSlotEntry> slotEntries_;
    // std::map<std::string /* slot name */, PipelineSlot> slots_;

    bool executed_ = false;

    friend class PipelineBuilder;
};

}  // namespace ujcore
