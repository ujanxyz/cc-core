#include "ujcore/utils/IdUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace {

static constexpr char kBase62Chars[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

std::string to_kBase62Chars(uint32_t value) noexcept {
    if (value == 0) return "";

    std::string result;
    while (value > 0) {
        result.push_back(kBase62Chars[value % 62]);
        value /= 62;
    }
    std::reverse(result.begin(), result.end());

    // Fix output length to 6 chars:
    return std::string(6 - result.size(), '0') + result;
}

uint64_t from_kBase62Chars(const std::string& str) noexcept {
uint32_t result = 0;
    for (char c : str) {
        const char* p = std::find(std::begin(kBase62Chars), std::end(kBase62Chars), c);
        if (p == std::end(kBase62Chars)) return 0;
        result = static_cast<uint32_t>(result * 62 + (p - kBase62Chars));
    }
    return result;
}

class Feistel32 {
public:
    explicit Feistel32(uint32_t key) {
        // derive round keys
        for (int i = 0; i < ROUNDS; ++i) {
            key = splitmix32(key);
            round_keys[i] = static_cast<uint16_t>(key & 0xffffu);
        }
    }

    uint32_t encrypt(uint32_t x) const {
        uint16_t L = static_cast<uint16_t>(x >> 16);
        uint16_t R = static_cast<uint16_t>(x & 0xffffu);

        for (int i = 0; i < ROUNDS; ++i) {
            uint16_t newL = R;
            uint16_t newR = static_cast<uint16_t>(L ^ F(R, round_keys[i]));
            L = newL;
            R = newR;
        }

        return static_cast<uint32_t>((static_cast<uint32_t>(L) << 16) | R);
    }

    uint32_t decrypt(uint32_t x) const {
        uint16_t L = static_cast<uint16_t>(x >> 16);
        uint16_t R = static_cast<uint16_t>(x & 0xffffu);

        for (int i = ROUNDS - 1; i >= 0; --i) {
            uint16_t newR = L;
            uint16_t newL = static_cast<uint16_t>(R ^ F(L, round_keys[i]));
            L = newL;
            R = newR;
        }

        return static_cast<uint32_t>((static_cast<uint32_t>(L) << 16) | R);
    }

private:
    static constexpr int ROUNDS = 6;
    std::array<uint16_t, ROUNDS> round_keys;

    static uint16_t F(uint16_t x, uint16_t k) {
        uint32_t v = static_cast<uint32_t>(x ^ k);
        v ^= v >> 16;
        v *= 0x7feb352d;
        v ^= v >> 15;
        v *= 0x846ca68b;
        v ^= v >> 16;
        return static_cast<uint16_t>(v & 0xffffu);
    }

    static uint32_t splitmix32(uint32_t& x) {
        x += 0x9e3779b9u;
        uint32_t z = x;
        z = (z ^ (z >> 16)) * 0x85ebca6bu;
        z = (z ^ (z >> 13)) * 0xc2b2ae35u;
        return z ^ (z >> 16);
    }
};

Feistel32& GetFeistel32Instance() {
    static Feistel32 instance(0x89abcdefu);  // fixed key for deterministic encoding
    return instance;
}

} // namespace

namespace ujcore {

std::string EncodeStringId(const uint32_t id) {
    const uint32_t encrypted_id = GetFeistel32Instance().encrypt(id);
    return to_kBase62Chars(encrypted_id);
}

uint32_t DecodeStringId(const std::string& str) {
    const uint32_t encrypted_id = from_kBase62Chars(str);
    return GetFeistel32Instance().decrypt(encrypted_id);
}

}  // namespace ujcore

