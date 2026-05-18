#pragma once

#include <string>

enum class AttributeDataType {
    kUnknown = 0,

    // The encoded data type is a special case that is used for manually entered data or
    // uploaded files, where the raw data is stored as a string in the encodedData field
    // of the slot state. The actual data type can be inferred from the function uri and
    // the slot name.
    kEncoded,

    kColors,
    kFloatList,
    kPoints2D,
    kBitmap,
    kGeometry,
};

// Enum to string conversion.
std::string AttributeDataTypeToStr(AttributeDataType enumVal);

// String to enum conversion.
AttributeDataType AttributeDataTypeFromStr(const std::string& name);
