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
        std::shared_ptr<Bitmap> bitmap;
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
        std::string encodedSlotId;  // Encoded slot id.

        void setFromUri(const std::string& uri) {
            CHECK(storage != nullptr);
            storage->ref = uri;
        }

        Bitmap* createBitmap() {
            CHECK(storage != nullptr);
            CHECK(resourceCtx != nullptr);
            CHECK(storage->bitmap == nullptr) << "Bitmap already set for this attribute.";
            BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
            CHECK(bitmapPool != nullptr);
            storage->bitmap = bitmapPool->CreateNewBitmap(encodedSlotId, 60 /* width */, 60 /* height */, 4 /* bytesPerPixel */);
            CHECK(storage->bitmap != nullptr);
            return storage->bitmap.get();

            // Here you would create a bitmap and store it in the resource context's bitmap pool,
            // then set the storage->ref to a URI that can be used to retrieve the bitmap later.
            // For example:
            // Bitmap* bmp = resourceCtx->GetBitmapPool()->CreateBitmap(...);
            // storage->ref = resourceCtx->GetBitmapPool()->GetUriForBitmap(bmp);
        }

        void DoneBitmapEditing() {
            // If there are any finalization steps needed after editing the bitmap, they can be done here.
            // For example, if you need to mark the bitmap as ready for use or trigger any events, you can do that here.
        }
    };
};
