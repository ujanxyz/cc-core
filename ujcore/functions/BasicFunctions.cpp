#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/CommonAttributes.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

class EmitFloatFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/emit-float";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a float")
            .WithDesc("Emit a fixed float value.")
            .WithOutParam("v", AttributeDataType::kFloat)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new EmitFloatFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto vOut = GetOutParam<FloatAttr>(ctx, "v");
        vOut->setFloatValue(3.141596f);
        return true;
    }
};

//--------------------------------------------------------------------------------------------------
class EmitPoint2DFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/emit-point2d";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a 2D point")
            .WithDesc("Emit a 2D coordinate point (x,y).")
            .WithOutParam("p", AttributeDataType::kPoint2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new EmitPoint2DFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        ctx.DumpDebugInfo();
        auto pOut = GetOutParam<Point2DAttr>(ctx, "p");
        pOut->setXValue(150.5f);
        pOut->setYValue(250.5f);
        return true;
    }
};

//--------------------------------------------------------------------------------------------------
class MyFunc final : public FunctionBase {
public:
    static inline const char* uri = "/basic/displace-point";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Displace point")
            .WithDesc("Displace a 2D point along X-axis by a given delta.")
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
        auto pIn = GetInParam<Point2DAttr>(ctx, "p");
        auto dx = GetInParam<FloatAttr>(ctx, "dx");

        float dxVal = dx->asFloatValue();
        LOG(INFO) << "@ Value of dx = " << dx->asFloatValue();
        LOG(INFO) << "@ Value of p.x = " << pIn->getX();
        LOG(INFO) << "@ Value of p.y = " << pIn->getY();

        auto fpOut = GetOutParam<Point2DAttr>(ctx, "fp");
        fpOut->setXValue(pIn->getX() + dxVal);
        fpOut->setYValue(pIn->getY());
        return true;
    }
};

__attribute__((constructor)) void RegisterBasicFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitFloatFn::uri, EmitFloatFn::spec, EmitFloatFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitPoint2DFn::uri, EmitPoint2DFn::spec, EmitPoint2DFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        MyFunc::uri, MyFunc::spec, MyFunc::newInstance, __FILE__);
}

}  // namespace
