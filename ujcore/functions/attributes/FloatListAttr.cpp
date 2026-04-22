#include "ujcore/functions/attributes/FloatListAttr.h"

#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using json = ::nlohmann::json;

__attribute__((constructor)) void RegisterFloatListAttr() {
    auto enode = [](std::shared_ptr<void> data) -> std::string {
        auto* floatStore = static_cast<FloatListAttr::Storage*>(data.get());
        const std::vector<float>& fValues = floatStore->fValues;

        // Encode fValues as JSON string.
        json jsonArray = json::array();
        for (const float f : fValues) {
            jsonArray.push_back(f);
        }
        return jsonArray.dump();
    };

    auto decode = [](const std::string& encoded) -> std::shared_ptr<void> {
        auto floatStore = std::make_shared<FloatListAttr::Storage>();

        // Parse JSON string to get fValues.
        json jsonArray = json::parse(encoded);
        if (!jsonArray.is_array()) {
            return nullptr;  // Invalid format
        }
        for (const auto& item : jsonArray) {
            if (!item.is_number_float()) {
                return nullptr;  // Invalid format
            }
            floatStore->fValues.push_back(item.get<float>());
        }
        return floatStore;
    };

    auto& registry = ujcore::AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("floats", FILE_LINE)
        .SetLabel("Float List")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
}

}  // namespace
