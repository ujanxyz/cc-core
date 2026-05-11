#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ujcore/function/FunctionSpec.h"

namespace ujcore {

// Lightweight query builder over FunctionRegistry with chainable filters and pagination.
class FunctionLookup {
public:
    FunctionLookup() = default;

    // Restrict lookup to the specified URI list. Non-existent URIs are ignored.
    FunctionLookup& WithUriList(const std::vector<std::string>& uriList);

    // Restrict lookup to URIs that start with the given prefix.
    FunctionLookup& WithUriPrefix(const std::string& uriPrefix);

    // Restrict lookup to specs whose label or description contains the search text.
    FunctionLookup& WithTextContains(const std::string& textContains);

    // Restrict lookup to functions that have at least one parameter with this dtype.
    FunctionLookup& WithInputDType(AttributeDataType dtype);

    // Restrict lookup to functions that have at least one output parameter with this dtype.
    FunctionLookup& WithOutputDType(AttributeDataType dtype);

    // Apply pagination over matched results: skip `offset` entries, then return up to `pageSize`.
    FunctionLookup& WithPagination(size_t offset, size_t pageSize);

    // Fetch function specs from the global FunctionRegistry using all configured filters.
    std::vector<FunctionSpec> Fetch() const;

private:
    std::optional<std::vector<std::string>> uriList_;
    std::optional<std::string> uriPrefix_;
    std::optional<std::string> textContains_;

    std::optional<AttributeDataType> inputDType_;
    std::optional<AttributeDataType> outputDType_;

    std::optional<std::pair<size_t, size_t>> pagination_;
};

}  // namespace ujcore
