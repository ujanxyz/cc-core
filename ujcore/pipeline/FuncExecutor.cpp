#include "ujcore/pipeline/FuncExecutor.h"

#include <cassert>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/function/AttributeDataType.h"

namespace ujcore {

FuncExecutor::FuncExecutor(NodeId selfId, std::unique_ptr<FunctionBase> funcInstance)
        : selfId_(selfId), funcInstance_(std::move(funcInstance)) {
    functionCtx_ = std::make_unique<FunctionContext>(this);
}

AttributeData* FuncExecutor::LookupAttr(const std::string& slotName) {
    auto slotIter = slotEntries_.find(slotName);
    if (slotIter == slotEntries_.end()) {
        return nullptr;
    }
    return &slotIter->second.attribute;
}

ujfunc::FunctionReturn FuncExecutor::RunFunction() {
    CHECK(funcInstance_ != nullptr) << "Function instance is null";
    return funcInstance_->OnRun(*functionCtx_);
}

NodeId FuncExecutor::GetFunctionNodeId() const {
    return selfId_;
}

AttributeData* FuncExecutor::OnGetParam(FuncParamAccess access, const std::string& name) {
    auto slotIter = slotEntries_.find(name);
    if (slotIter == slotEntries_.end()) {
        return nullptr;
    }
    return &slotIter->second.attribute;
}

void FuncExecutor::LogFromFunc(std::string_view message) {
    LOG(INFO) << "[Node-" << 0 << "]: " << message;
}

ResourceContext* FuncExecutor::GetResourceContext() {
    return resourceCtx_;
}

} // namespace ujcore
