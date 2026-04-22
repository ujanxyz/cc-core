#include "ujcore/function/AttributeDataType.h"

#include <unordered_map>

std::string AttributeDataTypeToStr(AttributeDataType enumVal) {
    switch (enumVal) {
        case AttributeDataType::kColor:
            return "color";
        case AttributeDataType::kFloatList:
            return "floats";
        case AttributeDataType::kPoints2D:
            return "points2d";
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
        {"floats", AttributeDataType::kFloatList},
        {"points2d", AttributeDataType::kPoints2D},
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
