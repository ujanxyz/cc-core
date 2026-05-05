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
#include "ujcore/utils/status_macros.h"
#include <cstdint>

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
// Function: InvertBitmapFn - Takes an input bitmap and emit a new bitmap where
// the colors are inverted (e.g. rgb(255,0,0) becomes rgb(0,255,255)).

class InvertBitmapFn final : public FunctionBase {
public:
    static inline const char* uri = "/basic/invert-bitmap";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("Invert bitmap")
            .WithDesc("Invert the colors of a bitmap.")
            .WithInputParam("bmpIn", AttributeDataType::kBitmap)
            .WithOutParam("bmpOut", AttributeDataType::kBitmap)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new InvertBitmapFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    absl::StatusOr<bool> OnRun(FunctionContext& ctx) override {
        auto bmpIn = GetInParam<BitmapAttr>(ctx, "bmpIn");
        auto bmpOut = GetOutParam<BitmapAttr>(ctx, "bmpOut");

        ASSIGN_OR_RETURN(const Bitmap* bmpInPtr, bmpIn->readBitmap());
        Bitmap* bmpOutPtr = bmpOut->createBitmap();

        const uint8_t* inPixels = bmpInPtr->bytes();
        uint8_t* outPixels = bmpOutPtr->bytes();

        const int32_t width0 = bmpInPtr->width();
        const int32_t height0 = bmpInPtr->height();
        const int32_t width1 = bmpOutPtr->width();
        const int32_t height1 = bmpOutPtr->height();

        const int32_t minWidth = std::min(width0, width1);
        const int32_t minHeight = std::min(height0, height1);
        LOG(INFO) << "Inverting bitmap, input size: (" << width0 << "x" << height0 << "), output size: (" << width1 << "x" << height1 << "), processing area: (" << minWidth << "x" << minHeight << ")";

        // First clear the output bitmap to yellow.
        for (int y = 0; y < height1; ++y) {
            for (int x = 0; x < width1; ++x) {
                int idx = (y * width1 + x) * 4;  // Assuming 4 bytes per pixel (RGBA)
                outPixels[idx] = 255;      // R
                outPixels[idx + 1] = 255;  // G
                outPixels[idx + 2] = 0;    // B
                outPixels[idx + 3] = 255;  // A
            }
        }

        // Invert pixels from bmpIn and copy to bmpOut, only the overlapping rect (top-left).
        for (int y = 0; y < minHeight; ++y) {
            for (int x = 0; x < minWidth; ++x) {
                int srcIdx = (y * width0 + x) * 4;  // stride = source bitmap width
                int dstIdx = (y * width1 + x) * 4;  // stride = destination bitmap width
                outPixels[dstIdx] = 255 - inPixels[srcIdx];       // R
                outPixels[dstIdx + 1] = 255 - inPixels[srcIdx + 1]; // G
                outPixels[dstIdx + 2] = 255 - inPixels[srcIdx + 2]; // B
                outPixels[dstIdx + 3] = inPixels[srcIdx + 3];     // A (keep alpha unchanged)
            }
        }
        return true;
    }
private:
};

//------------------------------------------------------------------------------
__attribute__((constructor)) void RegisterBasicFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        EmitBitmapFn::uri, EmitBitmapFn::spec, EmitBitmapFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        DrawColorBandsFn::uri, DrawColorBandsFn::spec, DrawColorBandsFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        InvertBitmapFn::uri, InvertBitmapFn::spec, InvertBitmapFn::newInstance, __FILE__);
}

}  // namespace
