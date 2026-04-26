#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/BitmapAttr.h" // Assuming you have defined BitmapAttr in this header
#include "ujcore/functions/attributes/ColorsAttr.h"
#include "ujcore/functions/attributes/FloatListAttr.h"
#include "ujcore/functions/attributes/Points2DAttr.h"
#include "ujcore/base/Bitmap.h"

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

//------------------------------------------------------------------------------
// Function: EmitBitmapFn - emits a fixed bitmap value with 4 rectangles drawn on it.

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

//------------------------------------------------------------------------------
// Function: DrawColorBandsFn - Takes a colors attribute, an array of N colors,
// and emit a bitmap where the canvas is vertically divided into N bands, where
// the i-th band is filled with the i-th color.

class DrawColorBandsFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/draw-color-bands";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Draw color bands")
            .WithDesc("Draw vertical color bands on a bitmap based on the input colors.")
            .WithInputParam("palette", AttributeDataType::kColors)
            .WithOutParam("pic", AttributeDataType::kBitmap)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new DrawColorBandsFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto vColors = GetInParam<ColorsAttr>(ctx, "palette");
        auto vOut = GetOutParam<BitmapAttr>(ctx, "pic");
        vOut->setFromUri("/bmpuri-002");
        Bitmap* bmp = vOut->createBitmap();
        LOG(INFO) << "DrawColorBandsFn created bitmap with id: " << bmp->id() << ", width: " << bmp->width() << ", height: " << bmp->height();
        drawColorBands(bmp, vColors->asColorsSpan());
        bmp->flush();
        return true;
    }

private:
    void drawColorBands(Bitmap* bmp, const std::span<const ColorsAttr::PackedRGBA>& colors) {
        int32_t width = bmp->width();
        int32_t height = bmp->height();
        int32_t bytesPerPixel = bmp->bytesPerPixel();
        uint8_t* pixels = bmp->bytes();

        int32_t numColors = colors.size();
        int32_t bandWidth = width / numColors;

        for (int i = 0; i < numColors; ++i) {
            const ColorsAttr::PackedRGBA& color = colors[i];
            int32_t startX = i * bandWidth;
            int32_t endX = (i == numColors - 1) ? width : startX + bandWidth;

            for (int y = 0; y < height; ++y) {
                for (int x = startX; x < endX; ++x) {
                    int idx = (y * width + x) * bytesPerPixel;
                    pixels[idx] = (color >> 24) & 0xFF;  // R
                    pixels[idx + 1] = (color >> 16) & 0xFF;  // G
                    pixels[idx + 2] = (color >> 8) & 0xFF;  // B
                    pixels[idx + 3] = color & 0xFF;  // A
                }
            }
        }
    }
};

//------------------------------------------------------------------------------
__attribute__((constructor)) void RegisterBasicFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitBitmapFn::uri, EmitBitmapFn::spec, EmitBitmapFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        DrawColorBandsFn::uri, DrawColorBandsFn::spec, DrawColorBandsFn::newInstance, __FILE__);
}

}  // namespace
