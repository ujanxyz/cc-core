#include "ujcore/function/FunctionLookup.h"

#include <algorithm>
#include <unordered_set>

#include "ujcore/function/FunctionRegistry.h"

namespace ujcore {
namespace {

bool HasPrefix(const std::string& text, const std::string& prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool ContainsParamWithDType(
    const std::vector<FuncParamSpec>& params,
    const FuncParamAccess wantedAccess,
    const AttributeDataType wantedDType) {
    for (const FuncParamSpec& param : params) {
        if (param.dtype != wantedDType) {
            continue;
        }
        if (wantedAccess == FuncParamAccess::kUnknown || param.access == wantedAccess) {
            return true;
        }
    }
    return false;
}

}  // namespace

FunctionLookup& FunctionLookup::WithUriList(const std::vector<std::string>& uriList) {
    uriList_ = uriList;
    return *this;
}

FunctionLookup& FunctionLookup::WithUriPrefix(const std::string& uriPrefix) {
    uriPrefix_ = uriPrefix;
    return *this;
}

FunctionLookup& FunctionLookup::WithTextContains(const std::string& textContains) {
    textContains_ = textContains;
    return *this;
}

FunctionLookup& FunctionLookup::WithInputDType(const AttributeDataType dtype) {
    inputDType_ = dtype;
    return *this;
}

FunctionLookup& FunctionLookup::WithOutputDType(const AttributeDataType dtype) {
    outputDType_ = dtype;
    return *this;
}

FunctionLookup& FunctionLookup::WithPagination(const size_t offset, const size_t pageSize) {
    pagination_ = std::make_pair(offset, pageSize);
    return *this;
}

std::vector<FunctionSpec> FunctionLookup::Fetch() const {
    FunctionRegistry& registry = FunctionRegistry::GetInstance();
    const std::vector<std::string> allUris = registry.GetAllUris();

    std::unordered_set<std::string> uriSet;
    if (uriList_.has_value()) {
        uriSet.insert(uriList_->begin(), uriList_->end());
    }

    std::vector<FunctionSpec> matched;
    matched.reserve(allUris.size());

    for (const std::string& uri : allUris) {
        if (uriList_.has_value() && !uriSet.contains(uri)) {
            continue;
        }
        if (uriPrefix_.has_value() && !HasPrefix(uri, *uriPrefix_)) {
            continue;
        }

        std::unique_ptr<FunctionSpec> spec = registry.GetSpecFromUri(uri);
        if (spec == nullptr) {
            continue;
        }

        if (textContains_.has_value()) {
            const bool inLabel = spec->label.find(*textContains_) != std::string::npos;
            const bool inDesc = spec->desc.find(*textContains_) != std::string::npos;
            if (!inLabel && !inDesc) {
                continue;
            }
        }

        if (inputDType_.has_value() &&
            !ContainsParamWithDType(spec->params, FuncParamAccess::kUnknown, *inputDType_)) {
            continue;
        }
        if (outputDType_.has_value() &&
            !ContainsParamWithDType(spec->params, FuncParamAccess::kOutput, *outputDType_)) {
            continue;
        }

        matched.push_back(*spec);
    }

    size_t begin = 0;
    size_t end = matched.size();
    if (pagination_.has_value()) {
        begin = pagination_->first;
        if (begin >= matched.size()) {
            return {};
        }
        end = std::min(end, begin + pagination_->second);
    }

    return std::vector<FunctionSpec>(matched.begin() + begin, matched.begin() + end);
}

}  // namespace ujcore
