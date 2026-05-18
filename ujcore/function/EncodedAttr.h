#pragma once

#include <memory>
#include <string>

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"

// This attribute represents encoded data.
class EncodedAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kEncoded};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kEncoded;
    }

    struct Storage {
        std::string encodedData;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        const std::string* GetEncodedString() const {
            CHECK(storage != nullptr);
            return &storage->encodedData;
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setEncodedString(const std::string& encodedData) {
             CHECK(storage != nullptr);
             storage->encodedData = encodedData;
        }
    };
};