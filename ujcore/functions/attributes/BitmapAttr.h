#pragma once

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "ujcore/base/Bitmap.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/ResourceContext.h"

absl::StatusOr<std::string> GetBitmapAssetUriFromEncoded(const std::string& encoded);

// This attribute represents a bitmap resource, where the storage holds a reference to the bitmap
// and the actual Bitmap object is managed in the resource context's bitmap pool.
class BitmapAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kBitmap};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kBitmap;
    }

    struct Storage {
        // A URI that references the bitmap resource, e.g. "idb:/media/lake-12345.png" (for IndexedDb)
        std::optional<std::string> assetUri;

        // The bitmap object is stored in the resource context's bitmap pool.
        std::shared_ptr<Bitmap> bitmap = nullptr;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;
        ResourceContext* resourceCtx = nullptr;

        bool hasBitmap() const {
            return storage != nullptr && storage->bitmap != nullptr;
        }

        Bitmap* getBitmap() const {
            if (storage == nullptr) {
                return nullptr;
            }
            return storage->bitmap.get();
        }

        std::optional<std::string> getUri() const {
            CHECK(storage != nullptr);
            return storage->assetUri;
        }

        absl::StatusOr<Bitmap*> readBitmap() const {
            if (storage == nullptr || storage->bitmap == nullptr) {
                return absl::NotFoundError("Bitmap not found");
            }
            return storage->bitmap.get();
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;
        ResourceContext* resourceCtx = nullptr;
        std::string encodedSlotId;  // Encoded slot id.

        bool hasBitmap() const {
            return storage != nullptr && storage->bitmap != nullptr;
        }

        Bitmap* getBitmap() const {
            if (storage == nullptr) {
                return nullptr;
            }
            return storage->bitmap.get();
        }

        void setFromUri(const std::string& uri) {
            CHECK(storage != nullptr);
            storage->assetUri = uri;
        }

        Bitmap* createBitmap(IDimension dimension) {
            
            LOG(INFO) << "Creating bitmap for slotId: " << encodedSlotId << ", dimension: " << dimension.width << "x" << dimension.height;
            CHECK(storage != nullptr);
            CHECK(resourceCtx != nullptr);
            CHECK(storage->bitmap == nullptr) << "Bitmap already set for this attribute.";
            BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
            CHECK(bitmapPool != nullptr) << "Bitmap pool not found in resource context";
            storage->bitmap = bitmapPool->CreateNewBitmap(encodedSlotId, dimension, 4 /* bytesPerPixel */);
            CHECK(storage->bitmap != nullptr);
            return storage->bitmap.get();
        }

        void Capture() {
            if (storage->bitmap != nullptr) {
                const std::string slotIdStr = resourceCtx->GetSlotIdStr();
                std::cout << "[BitmapAttr] Capturing bitmap for slot: " << slotIdStr << std::endl;

                storage->bitmap->onCapture(Bitmap::CaptureInfo {
                    .slotIdStr = encodedSlotId,
                    .modeStr = "function",
                });
            }
        }
    };
};
