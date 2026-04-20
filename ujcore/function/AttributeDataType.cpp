#include "ujcore/function/AttributeDataType.h"

#include <unordered_map>

std::string AttributeDataTypeToStr(AttributeDataType enumVal) {
    switch (enumVal) {
        case AttributeDataType::kColor:
            return "color";
        case AttributeDataType::kFloat:
            return "float";
        case AttributeDataType::kPoint2D:
            return "point2d";
        case AttributeDataType::kBitmap:
            return "bitmap";
        case AttributeDataType::kGeometry:
            return "geom2d";
        case AttributeDataType::kUnknown:
            return "unnown";
    }
}

AttributeDataType AttributeDataTypeFromStr(const std::string& name) {
    static const auto* const lookupTable = new std::unordered_map<std::string, AttributeDataType>({
        {"color", AttributeDataType::kColor},
        {"float", AttributeDataType::kFloat},
        {"point2d", AttributeDataType::kPoint2D},
        {"bitmap", AttributeDataType::kBitmap},
        {"geom2d", AttributeDataType::kGeometry},
    });
    auto iter = lookupTable->find(name);
    if (iter == lookupTable->end()) {
        return AttributeDataType::kUnknown;
    } else {
        return iter->second;
    }
}
