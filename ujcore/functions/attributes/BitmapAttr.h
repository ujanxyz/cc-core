#pragma once

#include "absl/log/check.h"
#include "ujcore/base/Bitmap.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/ResourceContext.h"

class BitmapAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kBitmap};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kBitmap;
    }

    struct Storage {
        std::string ref;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;
        ResourceContext* resourceCtx = nullptr;


        std::string getUri() const {
            CHECK(storage != nullptr);
            return storage->ref;
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;
        ResourceContext* resourceCtx = nullptr;

        void setFromUri(const std::string& uri) {
            CHECK(storage != nullptr);
            storage->ref = uri;
        }

        std::shared_ptr<Bitmap> createBitmap() {
            CHECK(storage != nullptr);
            CHECK(resourceCtx != nullptr);
            BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
            CHECK(bitmapPool != nullptr);
            std::shared_ptr<Bitmap> bmp = bitmapPool->CreateNewBitmap(60 /* width */, 60 /* height */, 4 /* bytesPerPixel */);
            CHECK(bmp != nullptr);
            return bmp;


            // Here you would create a bitmap and store it in the resource context's bitmap pool,
            // then set the storage->ref to a URI that can be used to retrieve the bitmap later.
            // For example:
            // Bitmap* bmp = resourceCtx->GetBitmapPool()->CreateBitmap(...);
            // storage->ref = resourceCtx->GetBitmapPool()->GetUriForBitmap(bmp);
        }
    };
};
