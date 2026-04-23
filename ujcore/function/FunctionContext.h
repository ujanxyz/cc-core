#pragma once

#include <string_view>

#include "ujcore/function/AttributeData.h"
#include "ujcore/function/FuncParamAccess.h"
#include "ujcore/function/ResourceContext.h"

class FunctionContextParent {
public:
    virtual ~FunctionContextParent() = default;

    virtual AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) = 0;
    virtual void LogFromFunc(std::string_view message) = 0;
    virtual void DumpDebugInfoFromFunc() = 0;
    virtual ResourceContext* GetResourceContext() = 0;
};

class FunctionContext {
public:
    FunctionContext(FunctionContextParent* parent): parent_(parent) {}

    const AttributeData* GetInParam(const std::string& name) {
        return parent_->OnGetParam(FuncParamAccess::kInput, name);
    }

    AttributeData* GetOutParam(const std::string& name) {
        return parent_->OnGetParam(FuncParamAccess::kOutput, name);
    }

    AttributeData* GetInOutParam(const std::string& name) {
        return parent_->OnGetParam(FuncParamAccess::kInOut, name);
    }

    void LogInfo(std::string_view message) {
        parent_->LogFromFunc(message);
    }

    void DumpDebugInfo() {
        parent_->DumpDebugInfoFromFunc();
    }

    ResourceContext* GetResourceContext() {
        return parent_->GetResourceContext();
    }

private:
    FunctionContextParent* const parent_;
};
