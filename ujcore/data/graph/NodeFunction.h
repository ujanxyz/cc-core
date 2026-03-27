#pragma once

#include "ujcore/functions/NodeFuncExtentions.hpp"

#include <any>
#include <string>
#include <vector>

namespace ujcore::data {

// Describes the execution function used in a node.
struct NodeFunction {
  // The node kind. Possible values are: in, out, fn, op, lam
  // TODO: Ideally this should be enum, but made a string to support emscripten converison.
  // In future this can be made an enum after the emscripten tool supports enum.
  std::string kind;

  // Label of the function, default node names are constructed from it.
  std::string label;

  // Short description of the function.
  std::string description;

  // Extension data specific to the actual node type.
  // The contained type tust be one of the above defined structs, as determined by 'kind'.
  std::any ext;
};

}  // namespace ujcore::data
