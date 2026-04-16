#pragma once

// Input params are read-only, output params are created from scratch.
// Mutable params are both read and modified.
enum class FuncParamAccess {
    kUnknown = 0,
    kInput,   // Input
    kOutput,  // Output
    kInOut,   // Mutatable
};