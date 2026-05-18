#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/functions/attributes/BitmapAttr.h" // Assuming you have defined BitmapAttr in this header
#include "ujcore/base/Bitmap.h"

#include <emscripten/em_asm.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>

namespace {

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

EM_JS(emscripten::EM_VAL, JsOnAwaitWGpuWork, (emscripten::EM_VAL handle), {
    const data = Emval.toValue(handle);
    const { type, width, height, ptrPixels, srcWidth, srcHeight, srcPtrPixels } = data;
    const pixels = new Uint8Array(Module.HEAPU8.buffer, ptrPixels, width * height * 4);
    const taskData = { type, width, height, pixels };

    if (typeof srcWidth === 'number' && typeof srcHeight === 'number' && typeof srcPtrPixels === 'number') {
        taskData.srcWidth = srcWidth;
        taskData.srcHeight = srcHeight;
        taskData.srcPixels = new Uint8Array(Module.HEAPU8.buffer, srcPtrPixels, srcWidth * srcHeight * 4);
    }

    console.log("[WebGpuDemoFn] Received task:", taskData, Module.wgpuTaskPool);
    const workId = Module.wgpuTaskPool.registerTask(taskData);
    const retValue = {
        workId,
    };
    return Emval.toHandle(retValue);
});

//------------------------------------------------------------------------------
// Function: WebGpuDemoFn - A demo function that creates a bitmap and draws on
// it using CPU.

class WebGpuDemoFn final : public FunctionBase {
public:
    using OutType = BitmapAttr::OutParam;
    // std::invoke_result<GetOutParam<BitmapAttr>, const std::string&>::type;

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
        if (bmpOut_ == nullptr) {
            bmpOut_ = GetOutParam<BitmapAttr>(ctx, "pic");
            LOG(INFO) << "Creating bitmap for the first time.";
            // vOut->setFromUri("/bmpuri-001");
            Bitmap* bmp = bmpOut_->createBitmap(IDimension::MakeWH(1024, 1024));
            const IDimension dim = bmp->dimension();
            LOG(INFO) << "WebGpuDemoFn created bitmap with id: " << bmp->id() << ", width: " << dim.width << ", height: " << dim.height;
            LOG(INFO) << "Simulating long GPU operation by returning pending status on first attempt.";
            std::string workuri = registerAwait(bmp);
            return ctx.ReturnAwait("webgpu", workuri);
        }

        CHECK(bmpOut_->hasBitmap());
        Bitmap* bmp = bmpOut_->getBitmap();
        const IDimension dim = bmp->dimension();
        LOG(INFO) << "Reusing existing bitmap with id: " << bmp->id() << ", width: " << dim.width << ", height: " << dim.height;
        drawOnBitmap(bmp);
        bmpOut_->Capture();
        return ctx.ReturnDone();
    }

private:
     std::unique_ptr<OutType> bmpOut_;

    std::string registerAwait(Bitmap* bmp) {
        CHECK(bmp != nullptr);

        const IDimension dim = bmp->dimension();
        emscripten::val target = emscripten::val::object();
        target.set("type", "OUTPUT_ONLY");
        target.set("width", dim.width);
        target.set("height", dim.height);
        target.set("ptrPixels", reinterpret_cast<uintptr_t>(bmp->bytes()));

        emscripten::val retObj = emscripten::val::take_ownership(JsOnAwaitWGpuWork(target.as_handle()));
        emscripten::val workIdVal = retObj["workId"];
        if (workIdVal.isUndefined() || workIdVal.isNull()) {
            LOG(WARNING) << "JsOnAwaitWGpuWork result missing workId, using fallback";
            return "wgpu-fallback-workId";
        }
        return workIdVal.as<std::string>();
    }

    // Given a bitmap, draw an opaque cyan rectangle in it, modifying raw rgba pixels.
    void drawOnBitmap(Bitmap* bmp) {
            uint8_t* pixels = bmp->bytes();
            const IDimension dim = bmp->dimension();
            int32_t width = dim.width;
            int32_t height = dim.height;
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
// Function: WebGpuBitmapEffectFn - Takes an input bitmap and emits an output
// bitmap. On first pass, it registers an await task with both source and
// destination bitmap metadata for JS-side WebGPU processing.

class WebGpuBitmapEffectFn final : public FunctionBase {
public:
    using InType = BitmapAttr::InParam;
    using OutType = BitmapAttr::OutParam;

    static inline const char* uri = "/basic/webgpu-bitmap-effect";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("WebGpu Bitmap Effect")
            .WithDesc("A demo function that processes an input bitmap using GPU.")
            .WithInputParam("src", AttributeDataType::kBitmap)
            .WithOutParam("dst", AttributeDataType::kBitmap)
            .Detach();
    }

    static FunctionBase* newInstance() {
        return new WebGpuBitmapEffectFn();
    }

    bool OnInit(FunctionContext& ctx) override {
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        if (dstBmp_ == nullptr) {
            bmpIn_ = GetInParam<BitmapAttr>(ctx, "src");
            auto srcBitmapOr = bmpIn_->readBitmap();
            if (!srcBitmapOr.ok()) {
                return ctx.ReturnStatus(std::move(srcBitmapOr).status());
            }
            srcBmp_ = std::move(srcBitmapOr).value();

            bmpOut_ = GetOutParam<BitmapAttr>(ctx, "dst");
            dstBmp_ = bmpOut_->createBitmap(srcBmp_->dimension());
            const IDimension dstDim = dstBmp_->dimension();
            std::string workuri = registerAwait(srcBmp_, dstBmp_);
            return ctx.ReturnAwait("webgpu", workuri);
        } else {
            // CPU fallback/demo completion path after await is resolved.
            // invertToOutput(srcBmp_, dstBmp_);
            bmpOut_->Capture();
            return ctx.ReturnDone();
        }
    }

private:
    Bitmap* srcBmp_ = nullptr;
    Bitmap* dstBmp_ = nullptr;
    std::unique_ptr<InType> bmpIn_;
    std::unique_ptr<OutType> bmpOut_;

    std::string registerAwait(Bitmap* srcBmp, Bitmap* dstBmp) {
        CHECK(srcBmp != nullptr);
        CHECK(dstBmp != nullptr);

        const IDimension dstDim = dstBmp->dimension();
        const IDimension srcDim = srcBmp->dimension();
        emscripten::val target = emscripten::val::object();
        target.set("type", "INPUT_AND_OUTPUT");
        target.set("width", dstDim.width);
        target.set("height", dstDim.height);
        target.set("ptrPixels", reinterpret_cast<uintptr_t>(dstBmp->bytes()));

        // Additional source bitmap metadata for the input image task.
        target.set("srcWidth", srcDim.width);
        target.set("srcHeight", srcDim.height);
        target.set("srcPtrPixels", reinterpret_cast<uintptr_t>(srcBmp->bytes()));

        emscripten::val retObj = emscripten::val::take_ownership(JsOnAwaitWGpuWork(target.as_handle()));
        emscripten::val workIdVal = retObj["workId"];
        if (workIdVal.isUndefined() || workIdVal.isNull()) {
            LOG(WARNING) << "JsOnAwaitWGpuWork result missing workId, using fallback";
            return "wgpu-fallback-workId";
        }
        return workIdVal.as<std::string>();
    }

    void invertToOutput(const Bitmap* srcBmp, Bitmap* dstBmp) {
        CHECK(srcBmp != nullptr);
        CHECK(dstBmp != nullptr);

        const uint8_t* inPixels = srcBmp->bytes();
        uint8_t* outPixels = dstBmp->bytes();

        const IDimension srcDim = srcBmp->dimension();
        const IDimension dstDim = dstBmp->dimension();
        const int32_t srcWidth = srcDim.width;
        const int32_t srcHeight = srcDim.height;
        const int32_t dstWidth = dstDim.width;
        const int32_t dstHeight = dstDim.height;

        const int32_t minWidth = std::min(srcWidth, dstWidth);
        const int32_t minHeight = std::min(srcHeight, dstHeight);

        for (int32_t y = 0; y < dstHeight; ++y) {
            for (int32_t x = 0; x < dstWidth; ++x) {
                const int32_t idx = (y * dstWidth + x) * 4;
                outPixels[idx] = 0;
                outPixels[idx + 1] = 0;
                outPixels[idx + 2] = 0;
                outPixels[idx + 3] = 255;
            }
        }

        for (int32_t y = 0; y < minHeight; ++y) {
            for (int32_t x = 0; x < minWidth; ++x) {
                const int32_t srcIdx = (y * srcWidth + x) * 4;
                const int32_t dstIdx = (y * dstWidth + x) * 4;
                outPixels[dstIdx] = 255 - inPixels[srcIdx];
                outPixels[dstIdx + 1] = 255 - inPixels[srcIdx + 1];
                outPixels[dstIdx + 2] = 255 - inPixels[srcIdx + 2];
                outPixels[dstIdx + 3] = inPixels[srcIdx + 3];
            }
        }
    }
};

//------------------------------------------------------------------------------
__attribute__((constructor)) void RegisterWebGpuFunctions() {
    FunctionRegistry::GetInstance().RegisterFunction(
        WebGpuDemoFn::uri, WebGpuDemoFn::spec, WebGpuDemoFn::newInstance, __FILE__);
    FunctionRegistry::GetInstance().RegisterFunction(
        WebGpuBitmapEffectFn::uri, WebGpuBitmapEffectFn::spec, WebGpuBitmapEffectFn::newInstance, __FILE__);
}

} // namespace
