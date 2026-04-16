#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

class FloatAttr {
public:
    struct ThisStorage {
        float fValue = 0.f;
    };

    class InParam {
        public:
            explicit InParam(FunctionContext& ctx, const std::string& name) {
                const AttributeData* attr = ctx.GetInParam(name);
                LOG(INFO) << "GetInParam result - " << attr->DebugString();
                if (attr == nullptr) {
                    LOG(FATAL) << "Attr (in) not found: " << name;
                }
                if (attr->dtype != AttributeDataType::kFloat) {
                    LOG(FATAL) << "Attr dtype mismatch";
                }
                storage_ = std::static_pointer_cast<ThisStorage>(attr->data);
                CHECK(storage_ != nullptr) << name << ", " << AttributeDataTypeToStr(attr->dtype);
            }
    
            float asFloatValue() const {
                CHECK(storage_ != nullptr);
                return storage_->fValue;
            }
    
        private:
            std::shared_ptr<ThisStorage> storage_;
    };

    class OutParam {
        public:
            OutParam(FunctionContext& ctx, const std::string& name) {
                AttributeData* attr = ctx.GetOutParam(name);
                if (attr == nullptr) {
                    LOG(FATAL) << "Attr (out) not found: " << name;
                }
                attr->dtype = AttributeDataType::kFloat;

                std::shared_ptr<ThisStorage> storage = std::make_shared<ThisStorage>(0.f);
                attr->data = storage;
                storage_ = std::static_pointer_cast<ThisStorage>(attr->data);
            }

            void setFloatValue(float val) const {
                CHECK(storage_ != nullptr);
                storage_->fValue = val;
            }
        private:
            std::shared_ptr<ThisStorage> storage_;
    };
};

//--------------------------------------------------------------------------------------------------

class Point2DAttr {
public:
    struct ThisStorage {
        float xValue = 0.f;
        float yValue = 0.f;
    };

    class InParam {
        public:
            explicit InParam(FunctionContext& ctx, const std::string& name) {
                const AttributeData* attr = ctx.GetInParam(name);
                if (attr == nullptr) {
                    LOG(FATAL) << "Attr (in) not found: " << name;
                }
                if (attr->dtype != AttributeDataType::kPoint2D) {
                    LOG(FATAL) << "Attr dtype mismatch";
                }
                storage_ = std::static_pointer_cast<ThisStorage>(attr->data);
                CHECK(storage_ != nullptr) << name << ", " << AttributeDataTypeToStr(attr->dtype);
            }
    
            float getX() const {
                CHECK(storage_ != nullptr);
                return storage_->xValue;
            }
    
            float getY() const {
                CHECK(storage_ != nullptr);
                return storage_->yValue;
            }

        private:
            std::shared_ptr<ThisStorage> storage_;
    };

    class OutParam {
        public:
            OutParam(FunctionContext& ctx, const std::string& name) {
                AttributeData* attr = ctx.GetOutParam(name);
                if (attr == nullptr) {
                    LOG(FATAL) << "Attr (out) not found: " << name;
                }
                attr->dtype = AttributeDataType::kPoint2D;

                std::shared_ptr<ThisStorage> storage = std::make_shared<ThisStorage>(ThisStorage {
                    .xValue = 0.f,
                    .yValue = 0.f,
                });
                attr->data = storage;
                storage_ = std::static_pointer_cast<ThisStorage>(attr->data);
            }

            void setXValue(float val) const {
                CHECK(storage_ != nullptr);
                storage_->xValue = val;
            }
            void setYValue(float val) const {
                CHECK(storage_ != nullptr);
                storage_->yValue = val;
            }
        private:
            std::shared_ptr<ThisStorage> storage_;
    };
};

//--------------------------------------------------------------------------------------------------
class TestEmitFloatFn final : public FunctionBase {
public:
    static inline const char* uri = "/testing/emit-float";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a float")
            .WithDesc("[Testing] Emit a fixed float value.")
            .WithOutParam("v", AttributeDataType::kFloat)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new TestEmitFloatFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        FloatAttr::OutParam vOut(ctx, "v");
        vOut.setFloatValue(3.141596f);

        return true;
    }
};

//--------------------------------------------------------------------------------------------------
class TestEmitPoint2DFn final : public FunctionBase {
public:
    static inline const char* uri = "/testing/emit-point2d";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a 2D point")
            .WithDesc("[Testing] Emit a 2D coordinate point (x,y).")
            .WithOutParam("p", AttributeDataType::kPoint2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new TestEmitPoint2DFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        ctx.DumpDebugInfo();

        Point2DAttr::OutParam pOut(ctx, "p");
        pOut.setXValue(150.5f);
        pOut.setYValue(250.5f);
        return true;
    }
};


//--------------------------------------------------------------------------------------------------
class MyFunc final : public FunctionBase {
public:
    static inline const char* uri = "/testing/displace-point";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("My Function")
            .WithDesc("This is my function.")
            .WithInputParam("p", AttributeDataType::kPoint2D)
            .WithInputParam("dx", AttributeDataType::kFloat)
            .WithOutParam("fp", AttributeDataType::kPoint2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new MyFunc();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        FloatAttr::InParam dx(ctx, "dx");
        Point2DAttr::InParam pIn(ctx, "p");
        float dxVal = dx.asFloatValue();
        LOG(INFO) << "@ Value of dx = " << dx.asFloatValue();
        LOG(INFO) << "@ Value of p.x = " << pIn.getX();
        LOG(INFO) << "@ Value of p.y = " << pIn.getY();

        Point2DAttr::OutParam fpOut(ctx, "fp");
        fpOut.setXValue(pIn.getX() + dxVal);
        fpOut.setYValue(pIn.getY());
        return true;
    }
};

__attribute__((constructor)) void RegisterMyFunc() {
    FunctionRegistry::GetInstance().RegisterFunction(
        TestEmitFloatFn::uri, TestEmitFloatFn::spec, TestEmitFloatFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        TestEmitPoint2DFn::uri, TestEmitPoint2DFn::spec, TestEmitPoint2DFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        MyFunc::uri, MyFunc::spec, MyFunc::newInstance, __FILE__);
}
