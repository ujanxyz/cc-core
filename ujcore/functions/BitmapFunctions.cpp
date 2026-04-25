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
        Bitmap* bmp = vOut->createBitmap();
        LOG(INFO) << "EmitBitmapFn created bitmap with id: " << bmp->id() << ", width: " << bmp->width() << ", height: " << bmp->height();
        drawOnBitmap(bmp);
        bmp->flush();
        return true;
    }

private:
    // Given a bitmap, draw an opaque cyan rectangle in it, modifying raw rgba pixels.
    void drawOnBitmap(Bitmap* bmp) {
            uint8_t* pixels = bmp->bytes();
            int32_t width = bmp->width();
            int32_t height = bmp->height();
            int32_t bytesPerPixel = bmp->bytesPerPixel();
    
            // Draw a cyan rectangle in the center of the bitmap.
            int32_t rectWidth = width / 2;
            int32_t rectHeight = height / 2;
            int32_t startX = (width - rectWidth) / 2;
            int32_t startY = (height - rectHeight) / 2;
    
            for (int y = startY; y < startY + rectHeight; ++y) {
                for (int x = startX; x < startX + rectWidth; ++x) {
                    int idx = (y * width + x) * bytesPerPixel;
                    pixels[idx] = 0;        // R
                    pixels[idx + 1] = 255;  // G
                    pixels[idx + 2] = 255;  // B
                    pixels[idx + 3] = 255;  // A
                }
            }
    }
};

__attribute__((constructor)) void RegisterBasicFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitBitmapFn::uri, EmitBitmapFn::spec, EmitBitmapFn::newInstance, __FILE__);
}

}  // namespace
