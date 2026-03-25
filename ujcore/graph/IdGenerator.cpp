#include "ujcore/graph/IdGenerator.hpp"

#include <algorithm>
#include <cstdint>

namespace ujcore {
namespace {

constexpr size_t kMaxIdLength = 10;

uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

uint64_t random_at(uint64_t seed, uint64_t index) {
    return splitmix64(seed + index);
}

std::string to_base62(uint64_t value, size_t maxlen) noexcept {
    static const char charset[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string result;
    while (value > 0 && result.length() < maxlen) {
        uint64_t rem = value % 62;
        result.push_back(charset[rem]);
        value /= 62;
    }
    while (result.length() < maxlen) {
        result.push_back('0');
    }
    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace

IdGenerator::IdGenerator() : id_length_(kMaxIdLength) {}

void IdGenerator::SetIdLength(size_t length) {
    id_length_ = length;
}

std::string IdGenerator::GetNextId() {
    const uint64_t mix = random_at(0, internal_counter_);
    ++internal_counter_;
    return to_base62(mix, id_length_);
}

std::string GenSplitMix64OfLength(uint64_t counter, size_t id_length) {
    const uint64_t mix = splitmix64(counter);
    return to_base62(mix, id_length);
}

}  // namespace ujcore
