#pragma once

#include <string>
#include <vector>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"

namespace ujcore {

// Stores minimal info about a function, used in the JS side.
struct FunctionInfo {
  /// @json: Encode as enum string, possible values: "I" | "O" | "M"
  enum class AccessEnum {
    I = 0,  // Input (read-only)
    O = 1,  // Output (created from scratch)
    M = 2,  // Mutable (read and modified, a.k.a. inout)
  };

  // A single parameter of this function.
  struct Param {
    // Local parameter name, e.g. "x", "radius".
    /// @json: Encode as string, like `"x"`
    std::string name;

    // Data type string, e.g. "floats", "bitmap", "points2d".
    /// @json: Encode as string, like `"floats"`
    std::string type;

    /// @json: Encode as enum string, see AccessEnum definition.
    AccessEnum access = AccessEnum::I;

    DEFINE_STRUCT_VISITOR_FUNCTION(name, type, access);
  };

  // The function uri, e.g. "/fn/geom/point2d-translate-x"
  std::string uri;

  // Label of the function, default node names are constructed from it.
  std::string label;

  // Verbose description of the function.
  std::string desc;

  // Input, output and mutable parameters of the function.
  std::vector<Param> params;

  DEFINE_ENUM_CONVERSION_FUNCTION(AccessEnum, I, O, M);
  DEFINE_STRUCT_VISITOR_FUNCTION(uri, label, desc, params);
};

}  // namespace ujcore
