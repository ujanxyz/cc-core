#include "ujcore/functions/attributes/BitmapAttr.h"

#include <memory>

#include "nlohmann/json.hpp"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using json = ::nlohmann::json;
constexpr const char* kAssetUriFieldName = "assetUri";

__attribute__((constructor)) void RegisterBitmapAttr() {
    auto enode = [](std::shared_ptr<void> data, ResourceContext* resourceCtx) -> std::string {
        std::shared_ptr<BitmapAttr::Storage> bitmapStore = std::static_pointer_cast<BitmapAttr::Storage>(data);
        const std::optional<std::string>& assetUri = bitmapStore->assetUri;

        // For simplicity, we just return the URI as the encoded string.
        // In a real implementation, you might want to include more metadata or use a different encoding format.
        json jsonObj;
        if (assetUri.has_value()) {
            jsonObj[kAssetUriFieldName] = assetUri.value();
        } else {
            jsonObj[kAssetUriFieldName] = nullptr;
        }

        if (bitmapStore->bitmap != nullptr) {
            bitmapStore->bitmap->onCapture(resourceCtx->GetSlotIdStr());
        }
        return jsonObj.dump();
    };

    auto decode = [](const std::string& encoded, ResourceContext* resourceCtx) -> std::shared_ptr<void> {
        auto bitmapStore = std::make_shared<BitmapAttr::Storage>();

        // Parse JSON string to get the URI.
        json jsonObj = json::parse(encoded);
        if (!jsonObj.is_object() || !jsonObj.contains(kAssetUriFieldName)) {
            return nullptr;  // Field missing or invalid object
        }
        const auto& ref = jsonObj[kAssetUriFieldName];
        if (!ref.is_string()) {
            return nullptr;
        }
        const std::string assetUri = ref.get<std::string>();
        bitmapStore->assetUri = assetUri;
        const std::string slotIdStr = resourceCtx->GetSlotIdStr();
        BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
        bitmapStore->bitmap = bitmapPool->ReleaseStagedBitmap(slotIdStr, assetUri);
        bitmapStore->bitmap->onCapture(slotIdStr);
        return bitmapStore;
    };

    auto& registry = ujcore::AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("bitmap", FILE_LINE)
        .SetLabel("Bitmap")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
}

}  // namespace

absl::StatusOr<std::string> GetBitmapAssetUriFromEncoded(const std::string& encoded) {
    // Decode the encoded string to get the asset URI.
    json jsonObj = json::parse(encoded);
    if (!jsonObj.is_object() || !jsonObj.contains(kAssetUriFieldName)) {
        return absl::InvalidArgumentError("Invalid encoded bitmap attribute: missing assetUri field");
    }
    const auto& ref = jsonObj[kAssetUriFieldName];
    if (!ref.is_string()) {
        return absl::InvalidArgumentError("Invalid encoded bitmap attribute: assetUri field is not a string");
    }
    return ref.get<std::string>();
}
