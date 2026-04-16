#pragma once

#include "absl/status/statusor.h"
#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionSpec.h"

class FunctionBase {
public:
    virtual ~FunctionBase() = default;

    static std::unique_ptr<ujcore::FunctionSpec> spec() {
        return nullptr;
    }

    virtual bool OnInit(FunctionContext& ctx) = 0;
    virtual absl::StatusOr<bool> OnRun(FunctionContext& ctx) = 0;
};
