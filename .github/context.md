# C++ Node Graph Execution Engine

## Overview

* Builds an node-based execution graph from function defnitions and data flow connections, and executes it.

## Development Style

* API-first: headers (.h) are defined manually by human.
* Codex should *implement*, assuming the design described by the API.
* Read the API, the specs may not be perfect and contain discrepancy. Find those and suggest correctons and improvements.
* Document the API headers. Don't be too verbose. Include an example use in comment if it is a complex API.
* Try to *keep* the suggested API contract, unless the human accepts the suggestions. It is ok to add nice docs.

## Build System

* Uses Bazel (bazelmod).
* Each directory has a `BILD.bazel` file, containing:
  - `default_visibility = ["//:__subpackages__"],`
  - One build rule per library, having a pair of header (`.hpp`) and  source (`.cpp`).
  - Snake case file names. If the API is a class, use the same name in snake case.
    Example: A class `NodeCreator` is defined in `node_creator.hpp` and `node_creator.cpp`.

# Unit tests

* Always have unit test for libraries. The test file name will be like `node_creator_test.cpp`.
* Use GoogleTest framework for unit-testing. Links `@googletest//:gtest_main` in the test target.
* Contain the entire test code inside an anomymous namespace inside the app namespace.
