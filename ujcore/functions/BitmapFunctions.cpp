#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/BitmapAttr.h" // Assuming you have defined BitmapAttr in this header
#include "ujcore/functions/attributes/FloatListAttr.h"
#include "ujcore/functions/attributes/Points2DAttr.h"
#include "ujcore/base/Bitmap.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

class EmitBitmapFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/emit-bitmap";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Emit bitmap")
            .WithDesc("Emit a fixed bitmap value.")
            .WithOutParam("pic", AttributeDataType::kBitmap)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new EmitBitmapFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto vOut = GetOutParam<BitmapAttr>(ctx, "pic");
        vOut->setFromUri("/bmpuri-001");
        std::shared_ptr<Bitmap> bmp = vOut->createBitmap();
        LOG(INFO) << "EmitBitmapFn created bitmap with id: " << bmp->id() << ", width: " << bmp->width() << ", height: " << bmp->height();
        return true;
    }
};

__attribute__((constructor)) void RegisterBasicFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitBitmapFn::uri, EmitBitmapFn::spec, EmitBitmapFn::newInstance, __FILE__);
}

}  // namespace
