#pragma once

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionContext.h"

namespace internal {

// Trait to check if a param type uses resource context (i.e. has a field named 'resourceCtx'
// of type ResourceContext*).
template <typename T>
concept HasResourceCtxPtrField = requires(T t) {
    // Checks if t.resourceCtx exists and the type is ResourceContext*
    { t.resourceCtx } -> std::same_as<ResourceContext*&>;
};

template <typename T>
concept HasEncodedSlotId = requires(T t) {
    // Checks if t.encodedSlotId exists and the type is std::string
    { t.encodedSlotId } -> std::same_as<std::string&>;
};

}  // namespace internal

template <class T>
auto GetInParam(FunctionContext& ctx, const std::string& name) -> std::unique_ptr<typename T::InParam> {
    using StorageType = typename T::Storage;
    using InParamType = typename T::InParam;

    const AttributeData* attr = ctx.GetInParam(name);
    if (attr == nullptr) {
        LOG(FATAL) << "Attr (in) not found: " << name;
        return nullptr;
    }
    auto acceptTypes = T::acceptsTypes();
    bool accept = std::find(
        acceptTypes.begin(), acceptTypes.end(), attr->dtype) != acceptTypes.end();
    if (!accept) {
        LOG(FATAL) << "Attr dtype rejected: " << name << ", dtype: "
            << AttributeDataTypeToStr(attr->dtype);
        return nullptr;
    }

    auto param = std::make_unique<InParamType>();
    param->storage = std::static_pointer_cast<StorageType>(attr->data);
    if constexpr (internal::HasEncodedSlotId<InParamType>) {
        param->encodedSlotId = absl::StrCat(ctx.GetNodeId().value, ":", name);
    }
    if constexpr (internal::HasResourceCtxPtrField<InParamType>) {
        param->resourceCtx = ctx.GetResourceContext();
    }
    return param;
}

template <class T>
auto GetOutParam(FunctionContext& ctx, const std::string& name) -> std::unique_ptr<typename T::OutParam> {
    using StorageType = typename T::Storage;
    using OutParamType = typename T::OutParam;

    AttributeData* attr = ctx.GetOutParam(name);
    if (attr == nullptr) {
        LOG(FATAL) << "Attr (out) not found: " << name;
        return nullptr;
    }
    const AttributeDataType yieldsType = T::yieldsType();
    std::shared_ptr<StorageType> storage = std::make_shared<StorageType>();
    attr->dtype = yieldsType;
    attr->data = storage;

    auto param = std::make_unique<OutParamType>();
    param->storage = std::static_pointer_cast<StorageType>(attr->data);
    if constexpr (internal::HasEncodedSlotId<OutParamType>) {
        param->encodedSlotId = absl::StrCat(ctx.GetNodeId().value, ":", name);
    }
    if constexpr (internal::HasResourceCtxPtrField<OutParamType>) {
        param->resourceCtx = ctx.GetResourceContext();
    }

    return param;
}

