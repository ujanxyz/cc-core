#pragma once

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace ujcore {

struct Param {
    std::string name;
    std::string data_type;
};

// The following structs with a "..Ext" suffix in their name describe the extension fields for
// various types of. execution nodes in the pipeline graph.

// An external input to the graph. This could be manually entered data (like list of numbers,
// colors, coordinate lists etc). Or it could be uploaded files, like PNG, MP4 video, CSV,
// or ZIP file. There will be various unwrap functions available which can be used in the
// backend of this node and export the data available as an output slot.
struct InputNodeExt {
    std::string data_type;
    std::vector<std::string> accepts;
};

// An external input from the graph. This takes a piece of computed data and exports as
// asset like PNG, MP4 video, CSV, or ZIP file.
struct OutputNodeExt {
    std::string data_type;
    std::vector<std::string> accepts;
    std::vector<std::string> emits;
};

// A function graph node is a pure function (side-effect free) with 0 or more input params,
// and 1 or more output params.
struct FunctionNodeExt {
    // Zero or more input params.
    std::vector<Param> inputs;
    // One or more Output params.
    std::vector<Param> outputs;
};

// An operator graph node is a mutator stage (has side-effect) that operates on a single piece of
// data. Typically these are draw modules on canvas, modifiers on geometry mesh.
// The only in-out param is implicit. Additionally there are 0 or more named input params.
struct OperatorNodeExt {
    std::string data_type;
    // Zero or more input params.
    std::vector<Param> inputs;
};

// An operator graph node returns a wrapped function, which can be invoked by later stages
// like a lambda object. They can be stateful or stateless, finite or infinite.
// The output from this node is a functor or lambda object which can be used as an iterator
// (stateful), generator (can be infinite), or field function (takes coordinate as input).
// The only out param is implicit, which is a functor type. We specify the args types and the
// return type of the functor object. Like std::function<R(A1, A2, ...)> in C++.
// Additionally there are 0 or more named input params.
struct LambdaNodeExt {
    // The returned data type.
    std::string data_type;
    // The argument types.
    std::vector<std::string> arg_types;
    // Zero or more input params.
    std::vector<Param> inputs;
};

}  // namespace ujcore
