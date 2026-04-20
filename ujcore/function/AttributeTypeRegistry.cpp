#include "ujcore/function/AttributeTypeRegistry.h"

#include "absl/log/log.h"

namespace ujcore {

using TypeBuilder = AttributeTypeRegistry::TypeBuilder;

class TypeBuilderImpl : public TypeBuilder {
public:
    TypeBuilderImpl(const std::string& srcLoc) : srcLoc_(srcLoc) {}

    TypeBuilder& SetLabel(const std::string& label) override {
        label_ = label;
        return *this;
    }

    TypeBuilder& SetToEncodedFn(AttributeEncodeFn&& toEncoded) override {
        toEncodedFn_ = std::move(toEncoded);
        return *this;
    }

    TypeBuilder& SetFromEncodedFn(AttributeDecodeFn&& fromEncoded) override {
        fromEncodedFn_ = std::move(fromEncoded);
        return *this;
    }

private:
    const std::string srcLoc_;
    std::string label_;
    AttributeEncodeFn toEncodedFn_;
    AttributeDecodeFn fromEncodedFn_;

    friend class AttributeTypeRegistry;
};

/* static */
AttributeTypeRegistry& AttributeTypeRegistry::GetInstance() {
    static AttributeTypeRegistry instance;
    return instance;
}

TypeBuilder& AttributeTypeRegistry::MutableTypeBuilder(const std::string& dtypeName, const std::string& srcLoc) {
    AttributeDataType dtype = AttributeDataTypeFromStr(dtypeName);
    if (dtype == AttributeDataType::kUnknown) {
        LOG(FATAL) << "Unknown attribute data type name: " << dtypeName << ", srcLoc: " << srcLoc;
    }
    auto iter = registry_.find(dtype);
    if (iter != registry_.end()) {
        auto* prevEntry = static_cast<TypeBuilderImpl*>(iter->second.get());
        LOG(FATAL) << "Duplicate registration for attribute data type: " << dtypeName
            << ", existing srcLoc: " << prevEntry->srcLoc_ << ", new srcLoc: " << srcLoc;
    }
    auto builder = std::make_unique<TypeBuilderImpl>(srcLoc);
    TypeBuilder& builderRef = *builder;
    registry_[dtype] = std::move(builder);
    return builderRef;
}

std::vector<std::tuple<std::string /* typeName */, std::string /* label */>>
AttributeTypeRegistry::GetAllTypesAndLabels() const {
    std::vector<std::tuple<std::string, std::string>> result;
    result.reserve(registry_.size());
    for (const auto& [dtype, builder] : registry_) {
        auto* builderImpl = static_cast<TypeBuilderImpl*>(builder.get());
        result.emplace_back(AttributeDataTypeToStr(dtype), builderImpl->label_);
    }
    return result;
}

const AttributeEncodeFn* AttributeTypeRegistry::GetEncodFn(const std::string& dtypeName) const {
    AttributeDataType dtype = AttributeDataTypeFromStr(dtypeName);
    auto iter = registry_.find(dtype);
    if (iter == registry_.end()) {
        return nullptr;
    }
    auto* builderImpl = static_cast<TypeBuilderImpl*>(iter->second.get());
    return &builderImpl->toEncodedFn_;
}

const AttributeDecodeFn* AttributeTypeRegistry::GetDecodFn(const std::string& dtypeName) const {
    AttributeDataType dtype = AttributeDataTypeFromStr(dtypeName);
    auto iter = registry_.find(dtype);
    if (iter == registry_.end()) {
        return nullptr;
    }
    auto* builderImpl = static_cast<TypeBuilderImpl*>(iter->second.get());
    return &builderImpl->fromEncodedFn_;
}

void AttributeTypeRegistry::ClearRegistry() {
    registry_.clear();
}

}  // namespace ujcore
