#pragma once

#include "ujcore/function/AttributeData.h"

class FunctionContextParent {
public:
    virtual ~FunctionContextParent() = default;
    virtual AttributeData* GetRawAttr(AttributeDataType type, const char *name) = 0;
};

class FunctionContext {
public:
    FunctionContext(FunctionContextParent* parent): parent_(parent) {}

    template <typename T>
    auto GetInParam(const char* name);

    template <typename T>
    auto GetOutParam(const char* name);
private:
    FunctionContextParent* const parent_;
};
