#pragma once

#include <map>
#include <memory>
#include <string>

#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionSpec.h"

namespace ujcore {

class FunctionRegistry {
public:
    using CreateFunctionFn = FunctionBase*(*)();
    using GetSpecFn = std::unique_ptr<FunctionSpec>(*)();

    static FunctionRegistry& GetInstance();

    void RegisterFunction(const std::string& uri, GetSpecFn get_spec_fn, CreateFunctionFn create_fn, const std::string& srcLoc);

    std::unique_ptr<FunctionSpec> GetSpecFromUri(const std::string& funcUri);

    std::unique_ptr<FunctionBase> CreateFromUri(const std::string& funcUri);

    std::vector<std::string> GetAllUris() const;

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
