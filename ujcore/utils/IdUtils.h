#pragma once

#include <cstdint>
#include <string>

namespace ujcore {

/**
 * Encodes a numeric ID into an obfuscated, URL-safe Base62 string.
 *
 * The algorithm first applies a deterministic 64-bit Feistel permutation
 * (keyed with a fixed internal constant), then converts the permuted value
 * to Base62 characters [0-9A-Za-z].
 */
std::string EncodeStringId(uint64_t id);

/**
 * Decodes a Base62 string produced by EncodeStringId back to the original ID.
 *
 * The algorithm converts the string from Base62 to a 64-bit value and applies
 * the inverse Feistel permutation.
 */
uint64_t DecodeStringId(const std::string& str);

}  // namespace ujcore
