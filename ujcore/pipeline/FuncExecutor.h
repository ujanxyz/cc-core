#pragma once

#include <map>
#include <memory>

#include "ujcore/graph/IdTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FuncParamAccess.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionReturn.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ResourceContext.h"


namespace ujcore {

class FuncExecutor : public FunctionContextParent {
public:
    explicit FuncExecutor(
      NodeId selfId,
      std::unique_ptr<FunctionBase> funcInstance);

    AttributeData* LookupAttr(const std::string& slotName);

    // This should be called before execution.
    void SetResourceContext(ResourceContext* resourceCtx) {
        resourceCtx_ = resourceCtx;
    }

    // Execute the function stage in the pipeline.
    // Returns a rich function status for step execution.
    ujfunc::FunctionReturn RunFunction();

    // Methods implementing `FunctionContextParent`.
    NodeId GetFunctionNodeId() const override;
    AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) override;
    void LogFromFunc(std::string_view message) override;

    ResourceContext* GetResourceContext() override;

private:
public: // TODO: change to private after pipeline builder is implemented.
    enum class ExecutorType: int8_t {
        kUnknown = 0,
        kFunction,
        kEncode,
        kDecode,
    };

    // Holds data and references for a single slot.
    struct PerSlotEntry {
        // The data type specified in the slot info.
        AttributeDataType dtype = AttributeDataType::kUnknown;
        FuncParamAccess access = FuncParamAccess::kUnknown;
        AttributeData attribute;
    };


    const NodeId selfId_;
    ExecutorType type_ = ExecutorType::kUnknown;
    std::unique_ptr<FunctionBase> funcInstance_;
    std::unique_ptr<FunctionContext> functionCtx_;
    std::map<std::string /* slot name */, PerSlotEntry> slotEntries_;

    std::string debugName_;

    // This is a non-owning raw pointer to the resource context in the pipeline runner.
    // This is shared by all nodes in the same pipeline.
    ResourceContext* resourceCtx_ = nullptr;

    bool done_ = false;

    friend class PipelineBuilder;
};


} // namespace ujcore
