#pragma once

#include <string_view>

#include "ujcore/graph/IdTypes.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/FuncParamAccess.h"
#include "ujcore/function/FunctionReturn.h"
#include "ujcore/function/ResourceContext.h"

class FunctionContextParent {
public:
    virtual ~FunctionContextParent() = default;

    virtual ujcore::NodeId GetFunctionNodeId() const = 0;
    virtual AttributeData* OnGetParam(FuncParamAccess access, const std::string& name) = 0;
    virtual void LogFromFunc(std::string_view message) = 0;
    virtual void DumpDebugInfoFromFunc() = 0;
    virtual ResourceContext* GetResourceContext() = 0;
};

class FunctionContext {
public:
    FunctionContext(FunctionContextParent* parent): parent_(parent) {}

    ujcore::NodeId GetNodeId() const {
        return parent_->GetFunctionNodeId();
    }

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

    // Helpers related to function return.

    // ---- DONE ----
    [[nodiscard]]
    ujfunc::FunctionReturn ReturnDone() const {
        return ujfunc::FunctionReturn{
        .code = ujfunc::ReturnCode::DONE
        };
    }

    // ---- NO DATA ----
    [[nodiscard]]
    ujfunc::FunctionReturn ReturnNoData(std::vector<std::string> missing) const {
        return ujfunc::FunctionReturn{
        .code = ujfunc::ReturnCode::NO_DATA,
        .noData = ujfunc::NoDataInfo{ std::move(missing) }
        };
    }

    // ---- AWAIT ----
    [[nodiscard]]
    ujfunc::FunctionReturn ReturnAwait(std::string channel,
                                std::string token) const {
        return ujfunc::FunctionReturn{
        .code = ujfunc::ReturnCode::AWAIT,
        .await = ujfunc::AwaitInfo{
            .channel = std::move(channel),
            .token = std::move(token),
        }
        };
    }

    // ---- ERROR: from absl::Status ----
    [[nodiscard]]
    ujfunc::FunctionReturn ReturnStatus(absl::Status&& status) const {
        if (status.ok()) {
            return ReturnDone();
        }
        return ujfunc::FunctionReturn{
            .code = ujfunc::ReturnCode::ERROR,
            .error = std::move(status),
        };
    }
  
  private:
    FunctionContextParent* const parent_;
};
