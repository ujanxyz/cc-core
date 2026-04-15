#pragma once

#include <map>
#include <memory>
#include <string>

#include "absl/log/log.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionSpec.h"

namespace ujcore {

class FunctionRegistry {
public:
    using CreateFunctionFn = FunctionBase*(*)();
    using GetSpecFn = std::unique_ptr<FunctionSpec>(*)();

    static FunctionRegistry& GetInstance() {
        static FunctionRegistry instance;
        return instance;
    }

    void RegisterFunction(const std::string& uri, GetSpecFn get_spec_fn, CreateFunctionFn create_fn, const std::string& srcLoc) {
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

    std::unique_ptr<FunctionSpec> GetSpecFromUri(const std::string& funcUri) {
        auto iter = registry_.find(funcUri);
        if (iter == registry_.end()) {
            LOG(ERROR) << "No function registered for uri: " << funcUri;
            return nullptr;
        }
        return iter->second.get_spec_fn();
    }

    std::unique_ptr<FunctionBase> CreateFromUri(const std::string& funcUri) {
        auto iter = registry_.find(funcUri);
        if (iter == registry_.end()) {
            LOG(ERROR) << "No function registered for uri: " << funcUri;
            return nullptr;
        }
        return std::unique_ptr<FunctionBase>(iter->second.create_fn());
    }

    std::vector<std::string> GetAllUris() const {
        std::vector<std::string> result;
        result.reserve(registry_.size());
        for (const auto& [uri, entry] : registry_) {
            result.push_back(uri);
        }        
        return result;
    }

private:
    // No public instantiation, only via GetInstance().
    FunctionRegistry() = default;

    struct FunctionEntry {
        GetSpecFn get_spec_fn;
        CreateFunctionFn create_fn;
        std::string srcLoc;
    };

    std::map<std::string /* uri */, FunctionEntry> registry_;
};

}  // namespace ujcore
