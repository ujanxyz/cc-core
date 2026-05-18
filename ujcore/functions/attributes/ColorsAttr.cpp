#include "ujcore/functions/attributes/ColorsAttr.h"

#include <string>
#include <charconv>
#include <cstdint>

#include "ujcore/function/EncodedAttr.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/ParamAccessors.h"
#include "absl/log/log.h"
#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;
using json = ::nlohmann::json;
using PackedRGBA = ColorsAttr::PackedRGBA;

std::string PackedRGBAToHexString(PackedRGBA color) {
    char buffer[10];  // Enough for "#RRGGBBAA" and null terminator
    if ((color & 0xFF) == 0xFF) {
        // If alpha is 255, we can use the shorter "#RRGGBB" format.
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
                      (color >> 24) & 0xFF,  // R
                      (color >> 16) & 0xFF,  // G
                      (color >> 8) & 0xFF);   // B
    } else {
        // Use the full "#RRGGBBAA" format.
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
                    (color >> 24) & 0xFF,  // R
                    (color >> 16) & 0xFF,  // G
                    (color >> 8) & 0xFF,   // B
                    color & 0xFF);         // A
    }
    return std::string(buffer);
}

std::optional<PackedRGBA> HexStringToPackedRGBA(const std::string& hexStr) {
    if (hexStr.empty()) {
        return std::nullopt;
    }
    if (hexStr[0] != '#') {
        return std::nullopt;  // Invalid format
    }
    if (hexStr.size() != 9 && hexStr.size() != 7) {
        LOG(ERROR) << "Invalid hex string length: " << hexStr;
        return std::nullopt;  // Invalid format
    }
    uint32_t result{};
    auto [ptr, ec] = std::from_chars(hexStr.data() + 1, hexStr.data() + hexStr.size(), result, 16);
    if (ec == std::errc()) {
        if (hexStr.size() == 7) {
            // If the format is #RRGGBB, assume alpha is FF.
            result = (result << 8) | 0xFF;
        } else {
            // If the format is #RRGGBBAA, the result is already correct.
        }
        return result;
    } else if (ec == std::errc::invalid_argument) {
        LOG(ERROR) << "Invalid hex string: " << hexStr;
        return std::nullopt;  // Parsing failed
    } else if (ec == std::errc::result_out_of_range) {
        LOG(ERROR) << "Hex string out of range: " << hexStr;
        return std::nullopt;  // Value out of range
    } else {
        LOG(ERROR) << "Unknown error parsing hex string: " << hexStr;
        return std::nullopt;  // Unknown error
    }
}


class ColorsDecodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$IN/colors";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Input colors")
            .WithDesc("Decodes a list of colors")
            .WithInputParam("$in", AttributeDataType::kEncoded)
            .WithOutParam("$out", AttributeDataType::kColors)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new ColorsDecodeFn();
    }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<EncodedAttr>(ctx, "$in");
        auto vOut = GetOutParam<ColorsAttr>(ctx, "$out");
        const std::string* encodedStr = vIn->GetEncodedString();
        CHECK(encodedStr != nullptr) << "Encoded input is null";

        json jsonArray = json::parse(*encodedStr);
        CHECK(jsonArray.is_array()) << "Invalid encoded colors: expected JSON array";

        std::vector<PackedRGBA> colors;
        colors.reserve(jsonArray.size());
        for (const auto& item : jsonArray) {
            CHECK(item.is_string()) << "Invalid encoded colors: expected hex color string entries";
            const std::string hexStr = item.get<std::string>();
            const auto colorOpt = HexStringToPackedRGBA(hexStr);
            CHECK(colorOpt.has_value()) << "Invalid color string: " << hexStr;
            colors.push_back(*colorOpt);
        }

        vOut->setFromPackedRGBASpan(colors);
        return ctx.ReturnDone();
    }
};


class ColorsEncodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$OUT/colors";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Output colors")
            .WithDesc("Encodes a list of colors")
            .WithInputParam("$in", AttributeDataType::kColors)
            .WithOutParam("$out", AttributeDataType::kEncoded)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new ColorsEncodeFn();
    }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<ColorsAttr>(ctx, "$in");
        auto vOut = GetOutParam<EncodedAttr>(ctx, "$out");

        json jsonArray = json::array();
        for (const PackedRGBA& color : vIn->asColorsSpan()) {
            jsonArray.push_back(PackedRGBAToHexString(color));
        }
        vOut->setEncodedString(jsonArray.dump());
        return ctx.ReturnDone();
    }
};

__attribute__((constructor)) void RegisterColorsAttr() {
    FunctionRegistry::GetInstance().RegisterFunction(
        ColorsDecodeFn::uri, ColorsDecodeFn::spec, ColorsDecodeFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        ColorsEncodeFn::uri, ColorsEncodeFn::spec, ColorsEncodeFn::newInstance, __FILE__);

    /// @deprecated: Remove after migrating to the above functions.
    // The JSON representation is an array of strings, like: ["#E0F08880", "#FF0000FF"]

    auto enode = [](std::shared_ptr<void> data, ResourceContext* resourceCtx) -> std::string {
        auto* floatStore = static_cast<ColorsAttr::Storage*>(data.get());
        const std::vector<PackedRGBA>& colors = floatStore->packedRGBAs;

        // Encode colors as JSON string.
        json jsonArray = json::array();
        for (const PackedRGBA& color : colors) {
            jsonArray.push_back(PackedRGBAToHexString(color));
        }
        return jsonArray.dump();
    };

    auto decode = [](const std::string& encoded, ResourceContext* resourceCtx) -> std::shared_ptr<void> {
        auto colorsStore = std::make_shared<ColorsAttr::Storage>();

        // Parse JSON string to get colors.
        json jsonArray = json::parse(encoded);
        if (!jsonArray.is_array()) {
            LOG(ERROR) << "Expected JSON array for colors, got: " << encoded;
            return nullptr;  // Invalid format
        }
        for (const auto& item : jsonArray) {
            if (!item.is_string()) {
                LOG(ERROR) << "Expected string for color, got: " << item;
                return nullptr;  // Invalid format
            }
            std::string hexStr = item.get<std::string>();
            auto colorOpt = HexStringToPackedRGBA(hexStr);
            if (!colorOpt.has_value()) {
                LOG(ERROR) << "Invalid color string: " << hexStr;
                return nullptr;  // Invalid color string
            }
            colorsStore->packedRGBAs.push_back(colorOpt.value());
        }
        return colorsStore;
    };

    auto& registry = ujcore::AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("colors", FILE_LINE)
        .SetLabel("Colors")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
}

}  // namespace
