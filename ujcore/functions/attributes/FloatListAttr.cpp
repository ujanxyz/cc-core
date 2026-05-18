#include "ujcore/functions/attributes/FloatListAttr.h"

#include "ujcore/function/EncodedAttr.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/ParamAccessors.h"

#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

#include "absl/strings/str_join.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;
using json = ::nlohmann::json;

class FloatListDecodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$IN/floats";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Input float list")
            .WithDesc("Decodes a list of float values")
            .WithInputParam("$in", AttributeDataType::kEncoded)
            .WithOutParam("$out", AttributeDataType::kFloatList)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new FloatListDecodeFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<EncodedAttr>(ctx, "$in");
        auto vOut = GetOutParam<FloatListAttr>(ctx, "$out");
        const std::string* encodedStr = vIn->GetEncodedString();
        CHECK(encodedStr != nullptr) << "Encoded input is null";

        LOG(INFO) << "Decoding float list from encoded string: " << *encodedStr;

        // Parse JSON string to get float values.
        json jsonArray = json::parse(*encodedStr);
        CHECK(jsonArray.is_array()) << "Invalid encoded float list: expected JSON array";

        std::vector<float> fValues;
        fValues.reserve(jsonArray.size());
        for (const auto& item : jsonArray) {
            CHECK(item.is_number_float()) << "Invalid encoded float list: expected float entries";
            fValues.push_back(item.get<float>());
        }
        vOut->setFromFloatSpan(fValues);
        LOG(INFO) << "Decoded float list: " << absl::StrJoin(fValues, ",");
        return ctx.ReturnDone();
    }
};

class FloatListEncodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$OUT/floats";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Output float list")
            .WithDesc("Encodes a list of float values")
            .WithInputParam("$in", AttributeDataType::kFloatList)
            .WithOutParam("$out", AttributeDataType::kEncoded)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new FloatListEncodeFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<FloatListAttr>(ctx, "$in");
        auto vOut = GetOutParam<EncodedAttr>(ctx, "$out");

        // Encode float values as JSON string.
        json jsonArray = json::array();
        for (const float f : vIn->asFloatSpan()) {
            jsonArray.push_back(f);
        }
        vOut->setEncodedString(jsonArray.dump());
        return ctx.ReturnDone();
    }
};

__attribute__((constructor)) void RegisterFloatListAttr() {
    FunctionRegistry::GetInstance().RegisterFunction(
        FloatListDecodeFn::uri, FloatListDecodeFn::spec, FloatListDecodeFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        FloatListEncodeFn::uri, FloatListEncodeFn::spec, FloatListEncodeFn::newInstance, __FILE__);

    /// @deprecated: Remove after migrating to the above functions.
    auto enode = [](std::shared_ptr<void> data, ResourceContext* resourceCtx) -> std::string {
        auto* floatStore = static_cast<FloatListAttr::Storage*>(data.get());
        const std::vector<float>& fValues = floatStore->fValues;

        // Encode fValues as JSON string.
        json jsonArray = json::array();
        for (const float f : fValues) {
            jsonArray.push_back(f);
        }
        return jsonArray.dump();
    };

    auto decode = [](const std::string& encoded, ResourceContext* resourceCtx) -> std::shared_ptr<void> {
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
