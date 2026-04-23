#include "ujcore/functions/attributes/BitmapAttr.h"

#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using json = ::nlohmann::json;

__attribute__((constructor)) void RegisterBitmapAttr() {
    auto enode = [](std::shared_ptr<void> data) -> std::string {
        auto* bitmapStore = static_cast<BitmapAttr::Storage*>(data.get());
        const std::string& ref = bitmapStore->ref;

        // For simplicity, we just return the URI as the encoded string.
        // In a real implementation, you might want to include more metadata or use a different encoding format.
        json jsonObj;
        jsonObj["ref"] = ref;
        return jsonObj.dump();
    };

    auto decode = [](const std::string& encoded) -> std::shared_ptr<void> {
        auto bitmapStore = std::make_shared<BitmapAttr::Storage>();

        // Parse JSON string to get the URI.
        json jsonObj = json::parse(encoded);
        if (!jsonObj.is_object() || !jsonObj.contains("ref")) {
            return nullptr;  // Field missing or invalid object
        }
        const auto& ref = jsonObj["ref"];
        if (!ref.is_string()) {
            return nullptr;  // Invalid field format
        }
        bitmapStore->ref = ref.get<std::string>();
        return bitmapStore;
    };

    auto& registry = ujcore::AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("bitmap", FILE_LINE)
        .SetLabel("Bitmap")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
}

}  // namespace
