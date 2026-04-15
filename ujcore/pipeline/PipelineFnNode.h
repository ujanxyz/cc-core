#pragma once

#include <map>
#include <memory>

#include "ujcore/function/AttributeData.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionSpec.h"

struct SlotStorage {
    enum class AccessKind {
        kUnknown = 0,
        kInput,
        kOutput,
        kInOut,
        kOverride, // Has manually overridden data.
    };
    AccessKind access = AccessKind::kUnknown;

    AttributeData attribute;

    // If true, this is a manually entered attribute, as opposed to one
    // coming through an incoming edge.
    // Manual attributes are prepared beforehand and only read execution,
    // they should not be cleared when the node is re-run.
    bool manual = false;

    // Tracks if the slot was accessed during the node execution.
    // Used to track progressive execution.
    bool accessed = false;
};

class PipelineFnNode : public FunctionContextParent {
public:

  static std::unique_ptr<PipelineFnNode> Create(
    const FunctionSpec& funcSpec,
      std::unique_ptr<FunctionBase> funcInstance);

  bool Init();

  AttributeData* GetRawAttr(AttributeDataType type, const char *name) override {
    // TODO: Implement this pure virtual method.
    return nullptr;
  }
private:
    std::unique_ptr<FunctionBase> funcInstance_;
    std::unique_ptr<FunctionContext> functionCtx_;
    std::map<std::string /* slot name */, SlotStorage> slots_;

    bool executed_ = false;

    friend class PipelineRunner;
};
