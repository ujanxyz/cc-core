#include "ujcore/function/FunctionRegistry.h"

#include "absl/log/log.h"

namespace ujcore {

using GetSpecFn = FunctionRegistry::GetSpecFn;

/* static */
FunctionRegistry& FunctionRegistry::GetInstance() {
    static FunctionRegistry instance;
    return instance;
}

void FunctionRegistry::RegisterFunction(
    const std::string& uri, GetSpecFn get_spec_fn, CreateFunctionFn create_fn, const std::string& srcLoc) {
    // Check for prior registered entry, if it was from the same source
    // location, silently ignore.
    auto iter = registry_.find(uri);
    if (iter != registry_.end()) {
        if (iter->second.srcLoc == srcLoc) {
            return;
        } else {
            LOG(FATAL) << "Duplicate registration: " << iter->second.srcLoc << ", " << srcLoc;
        }
    }
    FunctionEntry entry = {
        .get_spec_fn = get_spec_fn,
        .create_fn = create_fn,
        .srcLoc = srcLoc,
    };
    registry_[uri] = std::move(entry);
}

std::unique_ptr<FunctionSpec> FunctionRegistry::GetSpecFromUri(const std::string& funcUri) {
    auto iter = registry_.find(funcUri);
    if (iter == registry_.end()) {
        LOG(ERROR) << "No function registered for uri: " << funcUri;
        return nullptr;
    }
    return iter->second.get_spec_fn();
}

std::unique_ptr<FunctionBase> FunctionRegistry::CreateFromUri(const std::string& funcUri) {
    auto iter = registry_.find(funcUri);
    if (iter == registry_.end()) {
        LOG(ERROR) << "No function registered for uri: " << funcUri;
        return nullptr;
    }
    return std::unique_ptr<FunctionBase>(iter->second.create_fn());
}

std::vector<std::string> FunctionRegistry::GetAllUris() const {
    std::vector<std::string> result;
    result.reserve(registry_.size());
    for (const auto& [uri, entry] : registry_) {
        result.push_back(uri);
    }        
    return result;
}

}  // namespace ujcore
