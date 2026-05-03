#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FuncParamAccess.h"

namespace ujcore {

struct FuncParamSpec {
  // Local name, e.g. "x", "fx".
  std::string name;

  FuncParamAccess access = FuncParamAccess::kUnknown;

  // The data type. Left as free string for implementation. "[]T" denotes array of type T.
  // Common values are:
  // int, int[], float, []float.
  // The qualifier "T":
  // Geometric types like: 2D coordinates (pair of x,y), intervals are represented as "float2" (2 floats).
  // Rectangles are represented as "float4".
  // Colors are represented as "float3" (for RGB).
  AttributeDataType dtype = AttributeDataType::kUnknown;
};

// // The following structs with a "..Ext" suffix in their name describe the extension fields for
// // various types of functions in the pipeline graph.

// // An external input to the graph. This could be manually entered data (like list of numbers,
// // colors, coordinate lists etc). Or it could be uploaded files, like PNG, MP4 video, CSV,
// // or ZIP file. There will be various unwrap functions available which can be used in the
// // backend of this node and export the data available as an output slot.
// struct InputFuncExt {
//     std::string dtype;
//     std::vector<std::string> accepts;

//     DEFINE_STRUCT_VISITOR_FUNCTION(dtype, accepts);
// };

// // An external input from the graph. This takes a piece of computed data and exports as
// // asset like PNG, MP4 video, CSV, or ZIP file.
// struct OutputFuncExt {
//     std::string dtype;
//     std::vector<std::string> emits;

//     DEFINE_STRUCT_VISITOR_FUNCTION(dtype, emits);
// };

// // A pure function (side-effect free) graph node with 0 or more input params,
// // and 1 or more output params.
// struct PureFuncExt {
//     // Zero or more input params.
//     std::vector<ParamInfo> ins;
//     // One or more Output params.
//     std::vector<ParamInfo> outs;

//     DEFINE_STRUCT_VISITOR_FUNCTION(ins, outs);
// };

// // An operator graph node is a mutator stage (has side-effect) that operates on a single piece of
// // data. Typically these are draw modules on canvas, modifiers on geometry mesh.
// // The only in-out param is implicit. Additionally there are 0 or more named input params.
// struct OperatorFuncExt {
//     std::string dtype;
//     // Zero or more input params.
//     std::vector<ParamInfo> ins;

//     DEFINE_STRUCT_VISITOR_FUNCTION(dtype, ins);
// };

// // An lambda graph node returns a wrapped function, which can be invoked by later stages
// // like a lambda object. They can be stateful or stateless, finite or infinite.
// // The output from this node is a functor or lambda object which can be used as an iterator
// // (stateful), generator (can be infinite), or field function (takes coordinate as input).
// // The only out param is implicit, which is a functor type. We specify the args types and the
// // return type of the functor object. Like std::function<R(A1, A2, ...)> in C++.
// // Additionally there are 0 or more named input params.
// struct LambdaFuncExt {
//     // The returned data type.
//     std::string dtype;
//     // The argument data types.
//     std::vector<std::string> args;
//     // Zero or more input params.
//     std::vector<ParamInfo> ins;

//     DEFINE_STRUCT_VISITOR_FUNCTION(dtype, args, ins);
// };

// Describes the execution function used in a node.
struct FunctionSpec {
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

  // Input and output params.
  std::vector<FuncParamSpec> params;
};

/**
 * A builder for creating FunctionSpec instances.
 */
class FunctionSpecBuilder {
public:
    explicit FunctionSpecBuilder(const std::string& uri) {
        spec_ = std::make_unique<FunctionSpec>();
        spec_->uri = uri;
    }

    FunctionSpecBuilder& WithLabel(const std::string& label) {
        assert(!finalized_);
        spec_->label = label;
        return *this;
    }

    FunctionSpecBuilder& WithDesc(const std::string& desc) {
        assert(!finalized_);
        spec_->desc = desc;
        return *this;
    }

    FunctionSpecBuilder& WithInputParam(const std::string& name, const AttributeDataType dtype) {
        assert(!finalized_);
        spec_->params.push_back({name, FuncParamAccess::kInput, dtype});
        return *this;
    }

    FunctionSpecBuilder& WithOutParam(const std::string& name, const AttributeDataType dtype) {
        assert(!finalized_);
        spec_->params.push_back({name, FuncParamAccess::kOutput, dtype});
        return *this;
    }

    FunctionSpecBuilder& WithInOutParam(const std::string& name, const AttributeDataType dtype) {
        assert(!finalized_);
        spec_->params.push_back({name, FuncParamAccess::kInOut, dtype});
        return *this;
    }

    std::unique_ptr<FunctionSpec> Detach() {
        assert(!finalized_);
        finalized_ = true;
        return std::move(spec_);
    }

private:
    std::unique_ptr<FunctionSpec> spec_;
    bool finalized_ = false;
};

}  // namespace ujcore
