#pragma once

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/base/BitmapPool.h"

class BitmapAttribute {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kBitmap};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kBitmap;
    }

    struct Storage {
        std::shared_ptr<Bitmap> bitmap;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        const Bitmap* AsBitmap() const {
            CHECK(storage != nullptr);
            return storage->bitmap.get();
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        Bitmap* CreateBitmap() const {
            CHECK(storage != nullptr);
            auto newBitmap = GetBitmapPool().CreateNewBitmap(100, 100, 4);
            storage->bitmap = newBitmap;
            return storage->bitmap.get();
        }
    };
};