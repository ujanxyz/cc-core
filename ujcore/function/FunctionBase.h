#pragma once

#include "ujcore/function/FunctionContext.h"
#include "ujcore/function/FunctionReturn.h"
#include "ujcore/function/FunctionSpec.h"

class FunctionBase {
public:
    virtual ~FunctionBase() = default;

    static std::unique_ptr<ujcore::FunctionSpec> spec() {
        return nullptr;
    }

    virtual bool OnInit(FunctionContext& ctx) = 0;

    virtual ujfunc::FunctionReturn OnRun(FunctionContext& ctx) = 0;
};
