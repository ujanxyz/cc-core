#include "ujcore/pipeline/PipelineFnNode.h"

#include "absl/log/log.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/function/AttributeDataType.h"

namespace ujcore {

PipelineFnNode::PipelineFnNode(
        const NodeId nodeId,
        const FunctionSpec& funcSpec,
        std::unique_ptr<FunctionBase> funcInstance)
        : selfId_(nodeId),
        funcInstance_(std::move(funcInstance)) {
    functionCtx_ = std::make_unique<FunctionContext>(this);
    resourceCtx_ = std::make_unique<ResourceContext>();
}

PipelineSlot* PipelineFnNode::LookupSlot(const std::string& slotName) {
    auto slotIter = slots_.find(slotName);
    if (slotIter == slots_.end()) {
        return nullptr;
    }
    return &slotIter->second;
}

absl::StatusOr<bool> PipelineFnNode::RunFunction() {
    // Debug the attribute slot contents.
    for (auto& [slotName, slot] : slots_) {
        LOG(INFO) << "Running node " << selfId_.value << ", slot = " << slotName << " : "
            << static_cast<int>(slot.access) << ", dtype: "
            << AttributeDataTypeToStr(slot.attribute.dtype)
            << ", has_data: " << (slot.attribute.data != nullptr)
            << ", overridden: " << slot.overridden;
    }

    return funcInstance_->OnRun(*functionCtx_);
}

NodeId PipelineFnNode::GetFunctiontNodeId() const {
    return selfId_;
}

AttributeData* PipelineFnNode::OnGetParam(FuncParamAccess access, const std::string& name) {
    auto slotIter = slots_.find(name);
    if (slotIter == slots_.end()) {
        return nullptr;
    }
    PipelineSlot& slot = slotIter->second;
    if (slot.access != access) {
        LOG(ERROR) << "Access mismatch, at slot: " << name;
    }
    // TODO: Enable the following logic.
    // if (!slot.attribute.created) {
    //     LOG(ERROR) << "Slot data was not created: " << name;
    //     return nullptr;
    // }
    return &(slot.attribute);
}

void PipelineFnNode::LogFromFunc(std::string_view message) {
    LOG(INFO) << "[Node-" << selfId_.value << "]: " << message;
}

void PipelineFnNode::DumpDebugInfoFromFunc() {
    for (auto& [slotName, slot] : slots_) {
        LOG(INFO) << "    slot = " << slotName << " : "
            << static_cast<int>(slot.access) << ", dtype: "
            << AttributeDataTypeToStr(slot.attribute.dtype)
            << ", overridden: " << slot.overridden;
    }
}

ResourceContext* PipelineFnNode::GetResourceContext() {
    return resourceCtx_.get();
}

}  // namespace ujcore
