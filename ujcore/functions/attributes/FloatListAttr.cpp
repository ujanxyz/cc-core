#include "ujcore/functions/attributes/FloatListAttr.h"

#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/ParamAccessors.h"

#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;
using json = ::nlohmann::json;

class InputFloatList final : public FunctionBase {
public:
    static inline const char* uri = "/$IN/floats";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Input float list")
            .WithDesc("Accepts a float or list of float values")
            .WithOutParam("$out", AttributeDataType::kFloatList)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new InputFloatList();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vOut = GetOutParam<FloatListAttr>(ctx, "$out");
        vOut->setFromFloatSpan(std::vector<float>({0.123456f}));
        return ctx.ReturnDone();
    }
};

__attribute__((constructor)) void RegisterFloatListAttr() {
    FunctionRegistry::GetInstance().RegisterFunction(
        InputFloatList::uri, InputFloatList::spec, InputFloatList::newInstance, __FILE__);

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
