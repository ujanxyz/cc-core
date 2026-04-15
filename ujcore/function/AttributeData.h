#pragma once

#include <memory>

#include "ujcore/function/AttributeDataType.h"

struct AttributeData {
    AttributeDataType dtype = AttributeDataType::kUnknown;

    std::shared_ptr<void> data;

    // Track if this attribute was created during the node execution.
    // Used to track pregressive execution.
    bool created = false;
};