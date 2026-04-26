#pragma once

#include <span>

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"

// This attribute represents a list of floating-point values.
class FloatListAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kFloatList};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kFloatList;
    }

    struct Storage {
        std::vector<float> fValues;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        std::span<const float> asFloatSpan() const {
            CHECK(storage != nullptr);
            return std::span<const float>(storage->fValues.data(), storage->fValues.size());
        }

        float peekOrDefault(float defaultVal) const {
            CHECK(storage != nullptr);
            if (storage->fValues.size() > 0) {
                return storage->fValues[0];
            } else {
                return defaultVal;
            }
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setFromFloatSpan(std::span<const float> values) {
            CHECK(storage != nullptr);
            storage->fValues.assign(values.begin(), values.end());
        }
    };
};
