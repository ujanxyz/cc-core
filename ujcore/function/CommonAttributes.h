#pragma once

#include "absl/log/check.h"
#include "ujcore/function/AttributeDataType.h"

class FloatAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kFloat};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kFloat;
    }

    struct Storage {
        float fValue = 0.f;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        float asFloatValue() const {
            CHECK(storage != nullptr);
            return storage->fValue;
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setFloatValue(float val) const {
            CHECK(storage != nullptr);
            storage->fValue = val;
        }
    };
};

//--------------------------------------------------------------------------------------------------

class Point2DAttr {
public:
    static constexpr std::array<AttributeDataType, 1> acceptsTypes() {
        return {AttributeDataType::kPoint2D};
    }

    static constexpr AttributeDataType yieldsType() {
        return AttributeDataType::kPoint2D;
    }

    struct Storage {
        float xValue = 0.f;
        float yValue = 0.f;
    };

    struct InParam {
        std::shared_ptr<Storage> storage;

        float getX() const {
            CHECK(storage != nullptr);
            return storage->xValue;
        }

        float getY() const {
            CHECK(storage != nullptr);
            return storage->yValue;
        }
    };

    struct OutParam {
        std::shared_ptr<Storage> storage;

        void setXValue(float val) const {
            CHECK(storage != nullptr);
            storage->xValue = val;
        }
        void setYValue(float val) const {
            CHECK(storage != nullptr);
            storage->yValue = val;
        }
    };
};