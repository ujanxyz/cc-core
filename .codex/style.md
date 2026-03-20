# C++ Style Guide

## General

* Use C++20
* Use Google's C++ style guide, available at https://google.github.io/styleguide/cppguide.html

## Naming

* Local variables: `lower_case_with_underscores`
* Class names: Camel case, Example: `class NodeIdGenerator { ... };`
* Class member variables: `lowercase_with_a_trailing_underscore_`
* Global variables: Strongly discouraged. But it is OK to use static const variable in a local scope (to implement singleton or fixed lookup table). Those are names as `kInstance` or `kEnumToName`.
* Global constants can be used for POD types, if it makes the code better readable by avoiding magic constants: Examples: `constexpr int32_t kMaxRetryCount = 3;`, `constexpr const char kDefaultArgName[] = "...";          `
* Functions names: Camel case, Example: `void ParseInputArgs(...)`

## Headers

* Include what you use (IWYU). Avoid forward declarations when possible.
* Use `#pragma once` as header guard.
* Include order, separate the header includes in these 3 sections, separated by a blank line:
 - The *Primary header* file. In case of `.cc` / `.cpp` files, taht is the corresponding header. However there can be souce files without a header.
   Similarly for teat files, it is the library header. In case of header itself, the *Primary header* does not apply.
 - The system headers. Example: `#include <ctype>`, `#include <flat_map>`. Keep them sorted.
 - Library headers, use double quotes. Example: `#include "myproject/currentlib/Utils.hpp"`. Keep them sorted.
 - Add the copyright line `// Copyright 2026 (c) UjanXYZ` at the top of every file.

## Namespace

* Namescapes are minimal, prefer one level.
  - OK: `namespace ujcore { ... }`
  - Not OK: `namespace ujcore::builder:utils { ... }`
* In source files, place everything which do not have a header declaration inside an anonymous namespace.
* Comment at closing brace, like `}  // namespace ujcore`
* A blank line after the opening and before the closing brace.

## Memory

* Exceptions are strictly prohibited. In case of fatal errors, crash using `LOG(FATAL)` in `absl::logging`
* Prefer stack allocation
* No raw new/delete. Use smart pointers (prefer `unique_ptr` over `shared_ptr`).

## Formatting

* Use 2 char (space) indents, no tabs.
* Use 100 char line width.
* Use blank lines between functions, classes, namespaces, header groups, and at end-of-file.
* Opening brace (for `if`, `else`, `for`, `do` etc. blocks) in the same line.

## Comments

* Add doc comments for classes, header declaration of class methods, file-scoped functions inside the source files.
* Use only `// ...` inside code blocks. But keep comments at a minimal level, do not explain the obvious steps.

## Language features

* Prefer C++ range-based for loop, unless the loop index integer is actually needed.
* Prefer `std::tuple` over `std::pair`.
* Use structured decomposition. Examples:
 - `const auto [id, name, version] = GetIdNameVersionTuple();`
 - `for (const auto& [/* string */ edgeId, /* EdgeInfo */ info]: edgeInfoMap) {`
* Use designated initializers:
 - `idNameVersions.push_back({ .id = ..., .name = ..., .version = ...  })`

------------------------------------------------
## C++ HEADER example:

```c++
// File: ujcore/graph/NodeCreator.hpp
#pragma once

#include <ctypes>

namespace ujcore {

class NodeCreator {
public:
  std::string GetNextId();
};

}  // namespace ujcore
```

------------------------------------------------
## C++ Source example:


```c++
// File: ujcore/graph/NodeCreator.cpp
#include "ujcore/graph/NodeCreator.hpp"

#include <ctypes>
#include <vector>

#include "ujcore/utils/base64.h"
#include "ujcore/utils/splitmix.h"

namespace ujcore {
namespace {

std::string IdToString(uint64_t count) {
  return tobase64(splitmix(count));
}

}  // namespace

std::string NodeCreator::GetNextId() {
  return IdToString(count_++);
}

}  // namespace ujcore
```

------------------------------------------------
## C++ TEST example:

```c++
// File: ujcore/graph/NodeCreator_Test.cpp
#include "ujcore/graph/NodeCreator_Test.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"

namespace ujcore {

// Verify that the generated ids are deterministic.
TEST(NodeCreatorTest, DeterministicIds) {
  . . .
}

}  // namespace ujcore
```
