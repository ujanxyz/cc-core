#include "ujcore/base/IDimension.h"
#include "ujcore/functions/attributes/BitmapAttr.h"

#include <memory>

#include "nlohmann/json.hpp"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/base/IDimension.h"
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/EncodedAttr.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"
#include "ujcore/wasm/JsBitmapPool.h"

#include <emscripten/em_asm.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <string>

// These are function for encoding / decoding bitmap data type.
// Ideally these should be close to the type definition of `BitmapAttr`,
// but they depend on emscripten, so placed here.
// TODO: In future move is out using registration functions in BitmapAttr.cpp.

namespace {

using json = ::nlohmann::json;
constexpr const char* kAssetUriFieldName = "assetUri";

EM_JS(emscripten::EM_VAL, JsOnDecodingStart, (const char* cAssetUri, const char* cAssetId), {
    const assetUri = UTF8ToString(cAssetUri);
    const assetId = UTF8ToString(cAssetId);
    console.log("[JsOnDecodingStart] Received uri for decoding - assetUri:", assetUri);
    const taskId = Module.awaitPool.addTask("assetDb", { mode: "load", assetUri, assetId });
    return Emval.toHandle({ taskId });
});

EM_JS(emscripten::EM_VAL, JsOnDecodingResume, (const char* cTaskId), {
    const taskId = UTF8ToString(cTaskId);
    const taskResult = Module.awaitPool.releaseResult("assetDb", taskId, true /* forceDelete */);
    if (!taskResult) {
        console.warn("[JsOnDecodingResume] No result found for taskId:", taskId);
        return Emval.toHandle({ success: false });
    }
    const { assetId, imageData } = taskResult;
    const { width, height, data: pixelData, colorSpace } = imageData;   // pixelData is Uint8ClampedArray.
    const { byteLength, byteOffset } = pixelData;
    const numBytes = width * height * 4; // Assuming RGBA8 format

    // Allocate new memory on WASM heap. Wasm C++ cannot directly work on the underlying
    // ArrayBuffer of the ImageData, so we need to copy on to WASM-managed heap, and make
    // a new ImageBuffer out of it.
    const ptr = Module._malloc(numBytes);
    const uint8arr = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, numBytes);
    uint8arr.set(imageData.data);
    const newImgData = new ImageData(uint8arr, width, height, { colorSpace });


    console.log("[JsOnDecodingResume] Decoding completed for taskId:", taskId, "assetId:", assetId, "img:", newImgData);
    return Emval.toHandle({ success: true, width, height, numBytes, ptrPixels: ptr, imageData: newImgData });
});


EM_JS(emscripten::EM_VAL, JsOnEncodingStart, (emscripten::EM_VAL handle), {
    const target = Emval.toValue(handle);
    const {assetId, width, height, numBytes, ptrPixels} = target;
    const pixels = new Uint8Array(Module.HEAPU8.buffer, ptrPixels, numBytes);

    console.log("[JsOnEncodingStart] Received bitmap for encoding - width:", width, "height:", height, "pixels:", pixels);
    const taskId = Module.awaitPool.addTask("assetDb", { mode: "store", assetId, width, height, pixels });
    return Emval.toHandle({ taskId });
});

EM_JS(emscripten::EM_VAL, JsOnEncodingResume, (const char* taskIdStr), {
    const jTaskIdStr = UTF8ToString(taskIdStr);
    const taskResult = Module.awaitPool.releaseResult("assetDb", jTaskIdStr, true /* forceDelete */);
    if (taskResult) {
        const { assetId } = taskResult;
        console.log("[JsOnEncodingResume] Encoding completed for taskId:", jTaskIdStr, "assetId:", assetId);
        return Emval.toHandle({ success: true, assetId });
    } else {
        console.warn("[JsOnEncodingResume] No result found for taskId:", jTaskIdStr);
        return Emval.toHandle({ success: false });
    }
});

class BitmapDecodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$IN/bitmap";


    static std::unique_ptr<ujcore::FunctionSpec> spec() {
        return ujcore::FunctionSpecBuilder(uri)
            .WithLabel("Input bitmap")
            .WithDesc("Decodes a bitmap from encoded data")
            .WithInputParam("$in", AttributeDataType::kEncoded)
            .WithOutParam("$out", AttributeDataType::kBitmap)
            .Detach();
    }

    
    static FunctionBase* newInstance() { return new BitmapDecodeFn(); }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        if (inParam_ == nullptr) {
            inParam_ = GetInParam<EncodedAttr>(ctx, "$in");
            const std::string* encodedStr = inParam_->GetEncodedString();
            if (encodedStr == nullptr) {
                LOG(WARNING) << "Encoded input is null, returning error status";
                return ctx.ReturnNoData({"$in"});
            }
            const std::string assetId = absl::StrCat(ctx.GetNodeId().value, ":", "$out");
            std::string assetUri;
            {
                json jsonObj = json::parse(*encodedStr);
                CHECK(jsonObj.is_object() && jsonObj.contains(kAssetUriFieldName)) << "Missing assetUri field";
                const auto& ref = jsonObj[kAssetUriFieldName];
                CHECK(ref.is_string()) << "assetUri field is not a string";
                assetUri = ref.get<std::string>();
            }
            emscripten::val jsReturn = emscripten::val::take_ownership(JsOnDecodingStart(assetUri.c_str(), assetId.c_str()));
            awaitTaskId_ = jsReturn["taskId"].as<std::string>();  // Ensure taskId is a string

            return ctx.ReturnAwait("assetDb", awaitTaskId_.value());
        }
        else {
            CHECK(awaitTaskId_.has_value());
            emscripten::val jsReturn = emscripten::val::take_ownership(JsOnDecodingResume(awaitTaskId_->c_str()));
            const bool success = jsReturn["success"].as<bool>();
            if (!success) {
                return ctx.ReturnStatus(absl::InternalError("Resume error at BitmapDecodeFn"));
            }
            // Extract the fields from jsReturn.
            const int32_t width = jsReturn["width"].as<int32_t>();
            const int32_t height = jsReturn["height"].as<int32_t>();
            const intptr_t ptrPixels = jsReturn["ptrPixels"].as<intptr_t>();
            emscripten::val imageData = jsReturn["imageData"].as<emscripten::val>();

            uint8_t* const rawBytes = reinterpret_cast<uint8_t*>(ptrPixels);
            std::unique_ptr<uint8_t[]> pixelData = std::unique_ptr<uint8_t[]>(rawBytes);

            const std::string assetId = absl::StrCat(ctx.GetNodeId().value, ":", "$in");

            // Get the pool.
            ResourceContext* resourceCtx = ctx.GetResourceContext();
            BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
            std::shared_ptr<Bitmap> bitmap = bitmapPool->CreateAllocated(
                assetId, IDimension::MakeWH(width, height), std::move(pixelData));

            auto vOut = GetOutParam<BitmapAttr>(ctx, "$out");
            vOut->storage->assetUri = assetId;  // TODO: Use the URI from JS.
            vOut->storage->bitmap = std::move(bitmap);
            return ctx.ReturnDone();
        }

        // auto vIn = GetInParam<EncodedAttr>(ctx, "$in");
        // auto vOut = GetOutParam<BitmapAttr>(ctx, "$out");
        // const std::string* encodedStr = vIn->GetEncodedString();
        // CHECK(encodedStr != nullptr) << "Encoded input is null";

        // json jsonObj = json::parse(*encodedStr);
        // CHECK(jsonObj.is_object() && jsonObj.contains(kAssetUriFieldName)) << "Missing assetUri field";
        // const auto& ref = jsonObj[kAssetUriFieldName];
        // CHECK(ref.is_string()) << "assetUri field is not a string";
        // const std::string assetUri = ref.get<std::string>();

        // vOut->storage->assetUri = assetUri;
        // ResourceContext* resourceCtx = ctx.GetResourceContext();
        // const std::string slotIdStr = resourceCtx->GetSlotIdStr();
        // BitmapPool* bitmapPool = resourceCtx->GetBitmapPool();
        // vOut->storage->bitmap = bitmapPool->ReleaseStagedBitmap(slotIdStr, assetUri);
        // if (vOut->storage->bitmap != nullptr) {
        //     vOut->storage->bitmap->onCapture(Bitmap::CaptureInfo {
        //         .slotIdStr = slotIdStr,
        //         .modeStr = "decode",
        //     });
        // }
        // return ctx.ReturnDone();
    }

private:
    std::unique_ptr<EncodedAttr::InParam> inParam_;
    std::optional<std::string> awaitTaskId_;
};

class BitmapEncodeFn final : public FunctionBase {
public:
    static inline const char* uri = "/$OUT/bitmap";

    static std::unique_ptr<ujcore::FunctionSpec> spec() {
        return ujcore::FunctionSpecBuilder(uri)
            .WithLabel("Output bitmap")
            .WithDesc("Encodes a bitmap to encoded data")
            .WithInputParam("$in", AttributeDataType::kBitmap)
            .WithOutParam("$out", AttributeDataType::kEncoded)
            .Detach();
    }

    static FunctionBase* newInstance() { return new BitmapEncodeFn(); }

    bool OnInit(FunctionContext& ctx) override { return true; }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        if (inParam_ == nullptr) {
            InitialRun(ctx);
            return ctx.ReturnAwait("assetDb", awaitTaskId_.value());
        } else {
            CHECK(awaitTaskId_.has_value()) << "Await task ID is missing for resuming BitmapEncodeFn";
            emscripten::val jsReturn = emscripten::val::take_ownership(JsOnEncodingResume(awaitTaskId_->c_str()));
            const bool success = jsReturn["success"].as<bool>();
            if (!success) {
                return ctx.ReturnStatus(absl::InternalError("Resume error at BitmapEncodeFn"));
            }
            const std::string assetId = jsReturn["assetId"].as<std::string>();
            json jsonObj;
            jsonObj[kAssetUriFieldName] = assetId;
            auto vOut = GetOutParam<EncodedAttr>(ctx, "$out");
            vOut->setEncodedString(jsonObj.dump());

            return ctx.ReturnDone();
        }
    }

private:
    void InitialRun(FunctionContext& ctx) {
        inParam_ = GetInParam<BitmapAttr>(ctx, "$in");
        CHECK(inParam_->hasBitmap());
        Bitmap* bmp = inParam_->getBitmap();
        CHECK(bmp != nullptr);

        const IDimension dimension = bmp->dimension();
        const size_t numBytes = dimension.area() * 4; // Assuming 4 bytes per pixel (RGBA).
        const std::string assetId = absl::StrCat(ctx.GetNodeId().value, ":", "$in");

        emscripten::val target = emscripten::val::object();
        target.set("assetId", assetId);
        target.set("width", dimension.width);
        target.set("height", dimension.height);
        target.set("numBytes", numBytes);
        target.set("ptrPixels", reinterpret_cast<uintptr_t>(bmp->bytes()));
        emscripten::val jsReturn = emscripten::val::take_ownership(JsOnEncodingStart(target.as_handle()));
        awaitTaskId_ = jsReturn["taskId"].as<std::string>();
    }

    std::unique_ptr<BitmapAttr::InParam> inParam_;
    std::optional<std::string> awaitTaskId_;
};

__attribute__((constructor)) void RegisterBitmapAttr() {
    ::ujcore::FunctionRegistry::GetInstance().RegisterFunction(
        BitmapDecodeFn::uri, BitmapDecodeFn::spec, BitmapDecodeFn::newInstance, __FILE__);
    ::ujcore::FunctionRegistry::GetInstance().RegisterFunction(
        BitmapEncodeFn::uri, BitmapEncodeFn::spec, BitmapEncodeFn::newInstance, __FILE__);
}

}  // namespace
