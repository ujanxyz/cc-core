#pragma once

#include <string>

enum class AttributeDataType {
    kUnknown = 0,
    kColor,
    kFloatList,
    kPoints2D,
    kBitmap,
    kGeometry,
};

// Enum to string conversion.
std::string AttributeDataTypeToStr(AttributeDataType enumVal);

// String to enum conversion.
AttributeDataType AttributeDataTypeFromStr(const std::string& name);
