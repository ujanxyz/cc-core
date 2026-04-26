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
    auto slotIter = slotEntries_.find(slotName);
    if (slotIter == slotEntries_.end()) {
        return nullptr;
    }
    return slotIter->second.plSlot.get();
}

absl::StatusOr<bool> PipelineFnNode::RunFunction() {
    // Prior to execution, for input slots with manually overridden data, decode the encoded data to internal attribute.
    for (auto& [slotName, slotEntry] : slotEntries_) {
        if (slotEntry.decodeFnPtr != nullptr && slotEntry.encodedInput->has_value()) {
            const plstate::EncodedData& encodedData = slotEntry.encodedInput->value();
            std::shared_ptr<void> parsedData = (*slotEntry.decodeFnPtr)(encodedData.payload);
            if (parsedData == nullptr) {
                return absl::InternalError(absl::StrCat("Failed to decode manual override data for node ", selfId_.value, ", slot ", slotName));
            }
            auto& plSlot = *slotEntry.plSlot;
            plSlot.attribute = AttributeData {
                .dtype = slotEntry.dtype,
                .data = std::move(parsedData),
                .created = true,
            };
            plSlot.overridden = true;
        }
    }

    auto result = funcInstance_->OnRun(*functionCtx_);
    return result;
}

NodeId PipelineFnNode::GetFunctionNodeId() const {
    return selfId_;
}

AttributeData* PipelineFnNode::OnGetParam(FuncParamAccess access, const std::string& name) {
    auto slotIter = slotEntries_.find(name);
    if (slotIter == slotEntries_.end()) {
        return nullptr;
    }
    PipelineSlot& slot = *slotIter->second.plSlot;
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
    for (auto& [slotName, slotEntry] : slotEntries_) {
        auto& slot = *slotEntry.plSlot;
        LOG(INFO) << "    slot = " << slotName << " : "
            << static_cast<int>(slot.access) << ", dtype: "
            << AttributeDataTypeToStr(slot.attribute.dtype)
            << ", hasManual: " << slotEntry.encodedInput->has_value();
    }
}

ResourceContext* PipelineFnNode::GetResourceContext() {
    return resourceCtx_.get();
}

}  // namespace ujcore
