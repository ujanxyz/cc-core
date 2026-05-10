#include "ujcore/utils/IdUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace {

static constexpr char kBase62Chars[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

std::string to_kBase62Chars(uint64_t value) noexcept {
    if (value == 0) return "";

    std::string result;
    while (value > 0) {
        result.push_back(kBase62Chars[value % 62]);
        value /= 62;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

uint64_t from_kBase62Chars(const std::string& str) noexcept {
    uint64_t result = 0;
    for (char c : str) {
        const char* p = std::find(std::begin(kBase62Chars), std::end(kBase62Chars), c);
        if (p == std::end(kBase62Chars)) return 0;
        result = result * 62 + (p - kBase62Chars);
    }
    return result;
}

class Feistel64 {
public:
    explicit Feistel64(uint64_t key) {
        // derive round keys
        for (int i = 0; i < ROUNDS; ++i) {
            key = splitmix64(key);
            round_keys[i] = static_cast<uint32_t>(key);
        }
    }

    uint64_t encrypt(uint64_t x) const {
        uint32_t L = x >> 32;
        uint32_t R = x & 0xffffffff;

        for (int i = 0; i < ROUNDS; ++i) {
            uint32_t newL = R;
            uint32_t newR = L ^ F(R, round_keys[i]);
            L = newL;
            R = newR;
        }

        return (uint64_t(L) << 32) | R;
    }

    uint64_t decrypt(uint64_t x) const {
        uint32_t L = x >> 32;
        uint32_t R = x & 0xffffffff;

        for (int i = ROUNDS - 1; i >= 0; --i) {
            uint32_t newR = L;
            uint32_t newL = R ^ F(L, round_keys[i]);
            L = newL;
            R = newR;
        }

        return (uint64_t(L) << 32) | R;
    }

private:
    static constexpr int ROUNDS = 6;
    std::array<uint32_t, ROUNDS> round_keys;

    static uint32_t F(uint32_t x, uint32_t k) {
        uint32_t v = x ^ k;
        v ^= v >> 16;
        v *= 0x7feb352d;
        v ^= v >> 15;
        v *= 0x846ca68b;
        v ^= v >> 16;
        return v;
    }

    static uint64_t splitmix64(uint64_t& x) {
        x += 0x9e3779b97f4a7c15;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
};

Feistel64& GetFeistel64Instance() {
    static Feistel64 instance(0x12345678abcdef);  // example key, can be any value
    return instance;
}

} // namespace

namespace ujcore {

std::string EncodeStringId(const uint64_t id) {
    const uint64_t encrypted_id = GetFeistel64Instance().encrypt(id);
    return to_kBase62Chars(encrypted_id);
}

uint64_t DecodeStringId(const std::string& str) {
    const uint64_t encrypted_id = from_kBase62Chars(str);
    return GetFeistel64Instance().decrypt(encrypted_id);
}

}  // namespace ujcore

// TODO: Also add strong types from:
// #include "cppschema/common/strong_types.h"
