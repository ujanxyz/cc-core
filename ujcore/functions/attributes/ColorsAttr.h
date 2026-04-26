#pragma once

#include <span>

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"

// This attribute represents a list of colors, where each color is stored as a packed RGBA
// value in a uint32_t value.
class ColorsAttr {
public:
    using PackedRGBA = uint32_t;  // RGBA packed into a 32-bit unsigned integer.

    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kColors};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kColors;
    }

    struct Storage {
        std::vector<PackedRGBA> packedRGBAs;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        std::span<const PackedRGBA> asColorsSpan() const {
            CHECK(storage != nullptr);
            return std::span<const PackedRGBA>(storage->packedRGBAs.data(), storage->packedRGBAs.size());
        }

        PackedRGBA peekOrDefault() const {
            CHECK(storage != nullptr);
            if (storage->packedRGBAs.size() > 0) {
                return storage->packedRGBAs[0];
            } else {
                return static_cast<PackedRGBA>(0);  // default to 0 if no colors are available
            }
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setFromPackedRGBASpan(std::span<const PackedRGBA> values) {
            CHECK(storage != nullptr);
            storage->packedRGBAs.assign(values.begin(), values.end());
        }

        void appendPackedRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
             CHECK(storage != nullptr);
             const PackedRGBA packed = (static_cast<PackedRGBA>(r) << 24) |
                                      (static_cast<PackedRGBA>(g) << 16) |
                                      (static_cast<PackedRGBA>(b) << 8) |
                                      static_cast<PackedRGBA>(a);
             storage->packedRGBAs.push_back(packed);
        }
    };
};
