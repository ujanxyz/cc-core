#pragma once

#include <string>

#include "cppschema/common/visitor_macros.h"

namespace ujcore {

// Stores minimal info about a function, used in the JS side.
struct FunctionInfo {
  // The function uri, e.g. "/fn/geom/point2d-translate-x"
  std::string uri;

  // Label of the function, default node names are constructed from it.
  std::string label;

  // Verbose description of the function.
  std::string desc;

  DEFINE_STRUCT_VISITOR_FUNCTION(uri, label, desc);
};

}  // namespace ujcore
