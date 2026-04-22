#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/FloatListAttr.h"
#include "ujcore/functions/attributes/Points2DAttr.h"

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
            .WithOutParam("v", AttributeDataType::kFloatList)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new EmitFloatFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto vOut = GetOutParam<FloatListAttr>(ctx, "v");
        vOut->setFromFloatSpan(std::vector<float>({3.141596f}));
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
            .WithOutParam("p", AttributeDataType::kPoints2D)
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
        auto pOut = GetOutParam<Points2DAttr>(ctx, "p");
        pOut->appendPoint(150.5f, 250.5f);
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
            .WithInputParam("p", AttributeDataType::kPoints2D)
            .WithInputParam("dx", AttributeDataType::kFloatList)
            .WithOutParam("fp", AttributeDataType::kPoints2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new MyFunc();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto pIn = GetInParam<Points2DAttr>(ctx, "p");
        auto dx = GetInParam<FloatListAttr>(ctx, "dx");

        float dxVal = dx->peekOrDefault(0.f);
        const Points2DAttr::Point2D p = pIn->peekOrOrigin();
        LOG(INFO) << "@ Value of dx = " << dxVal;
        LOG(INFO) << "@ Value of p.x = " << p.x;
        LOG(INFO) << "@ Value of p.y = " << p.y;

        auto fpOut = GetOutParam<Points2DAttr>(ctx, "fp");
        fpOut->appendPoint(p.x + dxVal, p.y);
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
