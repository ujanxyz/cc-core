#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "cppschema/common/strong_types.h"

namespace ujcore {

/**
 * Encodes a numeric ID into an obfuscated, URL-safe Base62 string.
 *
 * The algorithm first applies a deterministic 32-bit Feistel permutation
 * (keyed with a fixed internal constant), then converts the permuted value
 * to Base62 characters [0-9A-Za-z].
 */
std::string EncodeStringId(uint32_t id);

/**
 * Works with any strong id with uint32_t as underlying. i.e. must be defined with:
 * DEFINE_STRONG_TYPE(Name, uint32_t)
 */
template<class Tag>
std::string EncodeStringId(const StrongType<uint32_t, Tag> id) {
    return EncodeStringId(id.value);
}

/**
 * Decodes a Base62 string produced by EncodeStringId back to the original ID.
 *
 * The algorithm converts the string from Base62 to a 32-bit value and applies
 * the inverse Feistel permutation.
 */
uint32_t DecodeStringId(const std::string& str);

template<class STRONG_TYPE>
STRONG_TYPE DecodeStringIdAsStrongType(const std::string& str) {
    static_assert(std::is_same_v<typename STRONG_TYPE::value_type, uint32_t>, "Strong type must have uint32_t as underlying type");
    return STRONG_TYPE(DecodeStringId(str));
}

}  // namespace ujcore
