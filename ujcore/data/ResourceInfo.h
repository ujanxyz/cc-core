#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/IdTypes.h"

namespace ujcore {

struct BitmapInfo {
  std::string id;  // The unique id of the bitmap resource.
  std::string backend;  // The backend used to create the bitmap, e.g. "skia", "webgl" etc.
  int32_t width = 0;
  int32_t height = 0;
  int32_t bytesPerPixel = 0;

  DEFINE_STRUCT_VISITOR_FUNCTION(id, backend, width, height, bytesPerPixel);
};

// Information about a pipeline resource created / used in the graph pipeline.
struct ResourceInfo {
  enum class Type {
    UNKNOWN = 0,
    BITMAP = 1,
  };

  // The resource type.
  Type type = Type::UNKNOWN;

  // Used if type == BITMAP. Contains information about the bitmap resource.
  std::optional<BitmapInfo> bitmap;

  DEFINE_ENUM_CONVERSION_FUNCTION(Type, UNKNOWN, BITMAP);
  DEFINE_STRUCT_VISITOR_FUNCTION(type, bitmap);
};

}  // namespace ujcore