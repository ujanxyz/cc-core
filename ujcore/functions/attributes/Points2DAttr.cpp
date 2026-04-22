#include "ujcore/functions/attributes/Points2DAttr.h"

#include "nlohmann/json.hpp"
#include "ujcore/function/AttributeTypeRegistry.h"

namespace {

using json = ::nlohmann::json;
using Point2D = Points2DAttr::Point2D;

__attribute__((constructor)) void RegisterPoints2DAttr() {
    auto enode = [](std::shared_ptr<void> data) -> std::string {
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

    auto decode = [](const std::string& encoded) -> std::shared_ptr<void> {
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
