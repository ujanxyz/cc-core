#pragma once

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"

#include <optional>
#include <string>
#include <vector>

namespace ujcore::data {

struct ParamInfo {
  // Local name, e.g. "x", "fx".
  std::string name;

  // The data type. Left as free string for implementation. "[]T" denotes array of type T.
  // Common values are:
  // int, int[], float, []float.
  // The qualifier "T"
  // Geometric types like: 2D coordinates (pair of x,y), intervals are represented as "float2" (2 floats).
  // Rectangles are represented as "float4".
  // Colors are represented as "float3" (for RGB).
  std::string dtype;

  DEFINE_STRUCT_VISITOR_FUNCTION(name, dtype);
};

// The following structs with a "..Ext" suffix in their name describe the extension fields for
// various types of functions in the pipeline graph.

// An external input to the graph. This could be manually entered data (like list of numbers,
// colors, coordinate lists etc). Or it could be uploaded files, like PNG, MP4 video, CSV,
// or ZIP file. There will be various unwrap functions available which can be used in the
// backend of this node and export the data available as an output slot.
struct InputFuncExt {
    std::string dtype;
    std::vector<std::string> accepts;

    DEFINE_STRUCT_VISITOR_FUNCTION(dtype, accepts);
};

// An external input from the graph. This takes a piece of computed data and exports as
// asset like PNG, MP4 video, CSV, or ZIP file.
struct OutputFuncExt {
    std::string dtype;
    std::vector<std::string> emits;

    DEFINE_STRUCT_VISITOR_FUNCTION(dtype, emits);
};

// A pure function (side-effect free) graph node with 0 or more input params,
// and 1 or more output params.
struct PureFuncExt {
    // Zero or more input params.
    std::vector<ParamInfo> ins;
    // One or more Output params.
    std::vector<ParamInfo> outs;

    DEFINE_STRUCT_VISITOR_FUNCTION(ins, outs);
};

// An operator graph node is a mutator stage (has side-effect) that operates on a single piece of
// data. Typically these are draw modules on canvas, modifiers on geometry mesh.
// The only in-out param is implicit. Additionally there are 0 or more named input params.
struct OperatorFuncExt {
    std::string dtype;
    // Zero or more input params.
    std::vector<ParamInfo> ins;

    DEFINE_STRUCT_VISITOR_FUNCTION(dtype, ins);
};

// An lambda graph node returns a wrapped function, which can be invoked by later stages
// like a lambda object. They can be stateful or stateless, finite or infinite.
// The output from this node is a functor or lambda object which can be used as an iterator
// (stateful), generator (can be infinite), or field function (takes coordinate as input).
// The only out param is implicit, which is a functor type. We specify the args types and the
// return type of the functor object. Like std::function<R(A1, A2, ...)> in C++.
// Additionally there are 0 or more named input params.
struct LambdaFuncExt {
    // The returned data type.
    std::string dtype;
    // The argument data types.
    std::vector<std::string> args;
    // Zero or more input params.
    std::vector<ParamInfo> ins;

    DEFINE_STRUCT_VISITOR_FUNCTION(dtype, args, ins);
};

// Defines the function kind, and the corresponding extension data.
struct FnExtendedInfo {
  // Possible types of function.
  enum class FnKind {
    _,
    INPUT,
    OUTPUT,
    PURE_FN,
    OPERATOR,
    LAMBDA,
  };

  // The node kind. Possible values are: in, out, fn, op, lam
  FnKind kind = FnKind::_;

  // Extensions specific to the function kinds. Must have exatly one of the following populated.
  std::optional<InputFuncExt> input;
  std::optional<OutputFuncExt> output;
  std::optional<PureFuncExt> purefn;
  std::optional<OperatorFuncExt> opfn;
  std::optional<LambdaFuncExt> lambda;

  DEFINE_ENUM_CONVERSION_FUNCTION(FnKind, _, INPUT, OUTPUT, PURE_FN, OPERATOR, LAMBDA);
  DEFINE_STRUCT_VISITOR_FUNCTION(kind, input, output,purefn, opfn, lambda);
};

// Describes the execution function used in a node.
struct FunctionInfo {
  // enum class ParamAccess {
  //   _,
  //   I, /* input */
  //   O, /* output */
  //   M, /* mutable */
  // };
  // DEFINE_ENUM_CONVERSION_FUNCTION(ParamAccess, _, I, O, M);

  // The function uri, e.g. "/fn/geom/point2d-translate-x"
  std::string uri;

  // Label of the function, default node names are constructed from it.
  std::string label;

  // Verbose description of the function.
  std::string desc;

  // Extension data depending on the kind of function.
  FnExtendedInfo ext;

  DEFINE_STRUCT_VISITOR_FUNCTION(uri, label, desc, ext);
};

}  // namespace ujcore::data
