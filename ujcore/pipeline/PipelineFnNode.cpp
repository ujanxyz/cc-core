#include "ujcore/pipeline/PipelineFnNode.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ujcore/graph/IdTypes.h"
#include "ujcore/function/AttributeDataType.h"

namespace ujcore {

PipelineFnNode::PipelineFnNode(
        const NodeId nodeId,
        const FunctionSpec& funcSpec,
        std::unique_ptr<FunctionBase> funcInstance)
        : selfId_(nodeId),
        funcInstance_(std::move(funcInstance)) {
    functionCtx_ = std::make_unique<FunctionContext>(this);
}

PipelineSlot* PipelineFnNode::LookupSlot(const std::string& slotName) {
    auto slotIter = slotEntries_.find(slotName);
    if (slotIter == slotEntries_.end()) {
        return nullptr;
    }
    return slotIter->second.plSlot.get();
}

ujfunc::FunctionReturn PipelineFnNode::RunFunction() {
    std::vector<std::string> missingSlots;

    // Prior to execution, for input slots with manually overridden data, decode the encoded data to internal attribute.
    for (auto& [slotName, slotEntry] : slotEntries_) {
        if (slotEntry.decodeFnPtr != nullptr && slotEntry.encodedInput->has_value()) {
            const grph::EncodedData& encodedData = slotEntry.encodedInput->value();
            std::shared_ptr<void> parsedData = (*slotEntry.decodeFnPtr)(encodedData.payload, resourceCtx_);
            if (parsedData == nullptr) {
                return ujfunc::FunctionReturn {
                    .code = ujfunc::ReturnCode::ERROR,
                    .error = absl::InternalError(
                        absl::StrCat("Failed to decode manual override data for node ", selfId_.value, ", slot ", slotName)),
                };
            }
            auto& plSlot = *slotEntry.plSlot;
            plSlot.attribute = AttributeData {
                .dtype = slotEntry.dtype,
                .data = std::move(parsedData),
                .created = true,
            };
            plSlot.overridden = true;
        }

        // Handle NO_DATA in wrapper layer, before function execution.
        auto& plSlot = *slotEntry.plSlot;
        if ((plSlot.access == FuncParamAccess::kInput || plSlot.access == FuncParamAccess::kInOut) &&
            plSlot.attribute.data == nullptr) {
            missingSlots.push_back(slotName);
        }
    }

    if (!missingSlots.empty()) {
        return ujfunc::FunctionReturn {
            .code = ujfunc::ReturnCode::NO_DATA,
            .noData = ujfunc::NoDataInfo { .missingSlots = std::move(missingSlots) },
        };
    }

    // resourceCtx_->SetSlotIdStr(absl::StrCat(selfId_.value, ":", "$out"));

    const ujfunc::FunctionReturn result = funcInstance_->OnRun(*functionCtx_);
    if (result.IsError() && !result.error.has_value()) {
        return ujfunc::FunctionReturn {
            .code = ujfunc::ReturnCode::ERROR,
            .error = absl::InternalError(
                absl::StrCat("Function returned ERROR without status for node ", selfId_.value)),
        };
    }
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
    return resourceCtx_;
}

}  // namespace ujcore
