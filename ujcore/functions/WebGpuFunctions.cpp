#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/BitmapAttr.h" // Assuming you have defined BitmapAttr in this header
#include "ujcore/base/Bitmap.h"
#include <cstdint>

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

//------------------------------------------------------------------------------
// Function: WebGpuDemoFn - A demo function that creates a bitmap and draws on
// it using CPU.

class WebGpuDemoFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/webgpu-demo";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("WebGpu Demo")
            .WithDesc("A demo function that creates a bitmap and draws on it using GPU.")
            .WithOutParam("pic", AttributeDataType::kBitmap)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new WebGpuDemoFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        ++attemptNumber_;
        LOG(INFO) << "WebGpuDemoFn attempt number: " << attemptNumber_;
        if (attemptNumber_ < 3) {
            LOG(INFO) << "Simulating long GPU operation by returning pending status on first attempt.";
            return ctx.ReturnAwait("webgpu", "test-001254");
        }
        auto vOut = GetOutParam<BitmapAttr>(ctx, "pic");
        vOut->setFromUri("/bmpuri-001");
        Bitmap* bmp = vOut->createBitmap();
        LOG(INFO) << "WebGpuDemoFn created bitmap with id: " << bmp->id() << ", width: " << bmp->width() << ", height: " << bmp->height();
        drawOnBitmap(bmp);
        vOut->Capture();
        return ctx.ReturnDone();
    }

private:
    int attemptNumber_ = 0;

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
                    pixels[idx] = 255;        // R
                    pixels[idx + 1] = 0;  // G
                    pixels[idx + 2] = 126;  // B
                    pixels[idx + 3] = 255;  // A
                }
            }
    }
};

//------------------------------------------------------------------------------
__attribute__((constructor)) void RegisterWebGpuFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        WebGpuDemoFn::uri, WebGpuDemoFn::spec, WebGpuDemoFn::newInstance, __FILE__);
}

} // namespace
