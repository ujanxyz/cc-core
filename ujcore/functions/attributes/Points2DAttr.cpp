#include "ujcore/functions/attributes/Points2DAttr.h"

#include "ujcore/function/EncodedAttr.h"
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
using Point2D = Points2DAttr::Point2D;


class Points2DDecodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$IN/points2d";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Input points 2D")
            .WithDesc("Decodes a list of points 2D")
            .WithInputParam("$in", AttributeDataType::kEncoded)
            .WithOutParam("$out", AttributeDataType::kPoints2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new Points2DDecodeFn();
    }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<EncodedAttr>(ctx, "$in");
        auto vOut = GetOutParam<Points2DAttr>(ctx, "$out");
        const std::string* encodedStr = vIn->GetEncodedString();
        CHECK(encodedStr != nullptr) << "Encoded input is null";

        json jsonArray = json::parse(*encodedStr);
        CHECK(jsonArray.is_array()) << "Invalid encoded points2d: expected JSON array";

        std::vector<Point2D> points;
        points.reserve(jsonArray.size());
        for (const auto& item : jsonArray) {
            CHECK(item.is_object()) << "Invalid encoded points2d: expected object entries";
            CHECK(item.contains("x") && item.contains("y"))
                << "Invalid encoded points2d: expected x and y";
            const auto& x = item["x"];
            const auto& y = item["y"];
            CHECK(x.is_number_float() && y.is_number_float())
                << "Invalid encoded points2d: x and y must be float";
            points.push_back(Point2D{x.get<float>(), y.get<float>()});
        }

        vOut->setFromPoint2DSpan(points);
        return ctx.ReturnDone();
    }
};


class Points2DEncodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$OUT/points2d";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Output points 2D")
            .WithDesc("Encodes a list of points 2D")
            .WithInputParam("$in", AttributeDataType::kPoints2D)
            .WithOutParam("$out", AttributeDataType::kEncoded)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new Points2DEncodeFn();
    }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        auto vIn = GetInParam<Points2DAttr>(ctx, "$in");
        auto vOut = GetOutParam<EncodedAttr>(ctx, "$out");

        json jsonArray = json::array();
        for (const Point2D& p : vIn->asSpan()) {
            json::object_t pointObj;
            pointObj["x"] = p.x;
            pointObj["y"] = p.y;
            jsonArray.push_back(std::move(pointObj));
        }
        vOut->setEncodedString(jsonArray.dump());
        return ctx.ReturnDone();
    }
};

__attribute__((constructor)) void RegisterPoints2DAttr() {
    FunctionRegistry::GetInstance().RegisterFunction(
        Points2DDecodeFn::uri, Points2DDecodeFn::spec, Points2DDecodeFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        Points2DEncodeFn::uri, Points2DEncodeFn::spec, Points2DEncodeFn::newInstance, __FILE__);

    /// @deprecated: Remove after migrating to the above functions.
    auto enode = [](std::shared_ptr<void> data, ResourceContext* resourceCtx) -> std::string {
        auto* floatStore = static_cast<Points2DAttr::Storage*>(data.get());
        const std::vector<Point2D>& points = floatStore->points;

        // Encode points as JSON string.
        json jsonArray = json::array();
        for (const Point2D& p : points) {
            json::object_t pointObj;
            pointObj["x"] = p.x;
            pointObj["y"] = p.y;
            jsonArray.push_back(std::move(pointObj));
        }
        return jsonArray.dump();
    };

    auto decode = [](const std::string& encoded, ResourceContext* resourceCtx) -> std::shared_ptr<void> {
        auto floatStore = std::make_shared<Points2DAttr::Storage>();

        // Parse JSON string to get points.
        json jsonArray = json::parse(encoded);
        if (!jsonArray.is_array()) {
            return nullptr;  // Invalid format
        }
        for (const auto& item : jsonArray) {
            auto& x = item["x"];
            auto& y = item["y"];
            if (!item.is_object() || !item.contains("x") || !item.contains("y") ||
                !x.is_number_float() || !y.is_number_float()) {
                return nullptr;  // Invalid format
            }
            floatStore->points.push_back(Point2D{x.get<float>(), y.get<float>()});
        }
        return floatStore;
    };

    auto& registry = ujcore::AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("points2d", FILE_LINE)
        .SetLabel("Points 2D")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
}

}  // namespace
