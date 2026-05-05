#include "ujcore/pipeline/PipelineIONode.h"

#include "absl/log/log.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"

namespace ujcore {

PipelineIONode::PipelineIONode(const NodeId nodeId, const bool isOutput)
    : selfId_(nodeId), isOutput_(isOutput) {
}

absl::Status PipelineIONode::RunAsIO() {
    if (isOutput_) {
        return InternalRunAsOutput();
    } else {
        return InternalRunAsInput();
    }
}

absl::StatusOr<plstate::GraphRunOutput> PipelineIONode::GetRunResult() const {
    std::optional<plstate::EncodedData> encodedData = std::nullopt;
    if (encodedOutput_.has_value()) {
        encodedData = plstate::EncodedData{.payload = encodedOutput_.value()};
    }
    return plstate::GraphRunOutput{
        .nodeId = selfId_,
        .dtype = AttributeDataTypeToStr(dtype_),
        .encodedData = std::move(encodedData),
    };
}

absl::Status PipelineIONode::InternalRunAsInput() {
    const AttributeDecodeFn* decodeFnPtr = std::get<const AttributeDecodeFn*>(convertFnPtr_);
    if (decodeFnPtr == nullptr) {
        return absl::InternalError("Invalid decode function pointer");
    }
    if (!encodedInput_->has_value()) {
        return absl::InternalError("Missing encoded payload for graph input");
    }
    resourceCtx_->SetSlotIdStr(absl::StrCat(selfId_.value, ":", "$out"));
    const std::string& encoded = encodedInput_->value().payload;
    VLOG(1) << "Decoding input for node " << selfId_.value << " with dtype " << AttributeDataTypeToStr(dtype_) << ", data: " << encoded;
    std::shared_ptr<void> decodedData = (*decodeFnPtr)(encoded, resourceCtx_);
    if (decodedData == nullptr) {
        return absl::InternalError("Failed to decode graph input data for node " + std::to_string(selfId_.value));
    }
    slot_.attribute = AttributeData {
        .dtype = dtype_,
        .data = std::move(decodedData),
        .created = true,
    };
    return absl::OkStatus();
}

absl::Status PipelineIONode::InternalRunAsOutput() {
    const AttributeEncodeFn* encodeFnPtr = std::get<const AttributeEncodeFn*>(convertFnPtr_);
    if (encodeFnPtr == nullptr) {
        return absl::InternalError("Invalid encode function pointer");
    }
    if (slot_.attribute.dtype != dtype_) {
        return absl::InternalError("Input attribute data type does not match node's data type");
    }
    resourceCtx_->SetSlotIdStr(absl::StrCat(selfId_.value, ":", "$in"));
    const std::string encoded = (*encodeFnPtr)(slot_.attribute.data, resourceCtx_);
    encodedOutput_ = encoded;
    return absl::OkStatus();
}

}  // namespace ujcore
