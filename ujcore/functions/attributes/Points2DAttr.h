#pragma once

#include <span>

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"

// 
class Points2DAttr {
public:
    struct Point2D {
        float x = 0.f;
        float y = 0.f;
    };

    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kPoints2D};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kPoints2D;
    }

    struct Storage {
        std::vector<Point2D> points;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        std::span<const Point2D> asSpan() const {
            CHECK(storage != nullptr);
            return std::span<const Point2D>(storage->points.data(), storage->points.size());
        }

        Point2D peekOrOrigin() const {
            CHECK(storage != nullptr);
            if (storage->points.size() > 0) {
                return storage->points[0];
            } else {
                return Point2D{};  // default to origin if no points are available
            }
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setFromPoint2DSpan(std::span<const Point2D> values) {
            CHECK(storage != nullptr);
            storage->points.assign(values.begin(), values.end());
        }

        void appendPoint(float x, float y) {
             CHECK(storage != nullptr);
             storage->points.push_back(Point2D{x, y});  
        }
    };
};
