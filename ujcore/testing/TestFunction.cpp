#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/FloatListAttr.h"
#include "ujcore/functions/attributes/Points2DAttr.h"

// Place the functions in a anonymous namespace, so if another file uses the
// same class name, it won't cause symbol conflict.
namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

class TestEmitFloatFn final : public FunctionBase {
public:
    static inline const char* uri = "/testing/emit-float";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a float")
            .WithDesc("[Testing] Emit a fixed float value.")
            .WithOutParam("v", AttributeDataType::kFloatList)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new TestEmitFloatFn();
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
class TestEmitPoint2DFn final : public FunctionBase {
public:
    static inline const char* uri = "/testing/emit-point2d";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit a 2D point")
            .WithDesc("[Testing] Emit a 2D coordinate point (x,y).")
            .WithOutParam("p", AttributeDataType::kPoints2D)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new TestEmitPoint2DFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto pOut = GetOutParam<Points2DAttr>(ctx, "p");
        pOut->appendPoint(150.5f, 250.5f);
        return true;
    }
};

//--------------------------------------------------------------------------------------------------
class MyFunc final : public FunctionBase {
public:
    static inline const char* uri = "/testing/displace-point";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Displace point")
            .WithDesc("[Testing] Displace a 2D point along X-axis by a given delta.")
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
        auto fpOut = GetOutParam<Points2DAttr>(ctx, "fp");
        fpOut->appendPoint(p.x + dxVal, p.y);
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

}  // namespace
