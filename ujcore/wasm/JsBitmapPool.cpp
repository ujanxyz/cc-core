#include "ujcore/wasm/JsBitmapPool.h"

#include <emscripten/em_asm.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <iostream>

#include "absl/log/check.h"
#include "ujcore/base/BackendRegistry.h"

namespace {

// Create a raw buffer on the WASM heap and return a pointer to it.
// The C++ side is will be writing pixel data into this buffer.
EM_JS(emscripten::EM_VAL, JsOnCreateBitmap, (emscripten::EM_VAL handle), {
    const target = Emval.toValue(handle);
    const {width, height, numBytes} = target;
    const ptr = Module._malloc(numBytes); // Allocate memory on WASM heap.
    const uint8arr = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, numBytes);
    const newImgData = new ImageData(uint8arr, width, height, { colorSpace: "srgb", pixelFormat: "rgba-unorm8" });

    const retValue = {
      dataPtr: ptr,
      imageData: newImgData,
    };
    return Emval.toHandle(retValue);
});

EM_JS(emscripten::EM_VAL, JsReleaseStagedBitmap, (const char* slotIdStr, const char* assetUri), {
  const jSlotIdStr = UTF8ToString(slotIdStr);
  const jAssetUri = UTF8ToString(assetUri);
  const imageData = Module.assetStaging.releaseImageData(jSlotIdStr, jAssetUri);
  if (!imageData) {
    console.warn("[JsBitmapPool] No staged bitmap found for slot: ", jSlotIdStr, " with asset URI: ", jAssetUri);
    return null;
  }

  const { width, height, data, colorSpace, pixelFormat } = imageData;
  if (colorSpace !== "srgb" || pixelFormat !== "rgba-unorm8") {
    console.error("[JsBitmapPool] Unsupported image data format: ", colorSpace, pixelFormat);
    return null;
  }
  const { byteLength, byteOffset } = data;

  const numBytes = width * height * 4; // Assuming RGBA8 format
  if (byteLength !== numBytes) {
    console.error("[JsBitmapPool] Mismatch in expected byte length: ", byteLength, " vs calculated: ", numBytes);
    return null;
  }

  // Allocate new memory on WASM heap. Wasm C++ cannot directly work on the underlying
  // ArrayBuffer of the ImageData, so we need to copy on to WASM-managed heap, and make
  // a new ImageBuffer out of it.
  const ptr = Module._malloc(numBytes);
  const uint8arr = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, numBytes);
  uint8arr.set(imageData.data);
  const newImgData = new ImageData(uint8arr, width, height, { colorSpace, pixelFormat });

  const retValue = {
    width,
    height,
    dataPtr: ptr,
    imageData: newImgData,
  };

  console.log("[JsBitmapPool] returning : ", retValue);
  return Emval.toHandle(retValue);
});

EM_JS(void, JsOnCaptureBitmap, (const char* slotIdStr, emscripten::EM_VAL imageData), {
    const jSlotIdStr = UTF8ToString(slotIdStr);
    const jImageData = Emval.toValue(imageData);
    globalThis.pipelineEvents.dispatchEvent(new CustomEvent("BITMAP_CAPTURED", { detail: { assetKey: jSlotIdStr, imageData: jImageData } }));
});

EM_JS(void, JsOnDestroyBitmap, (uint8_t* pixelData), {
    Module._free(pixelData);
});

class JsHeapBackedBitmap final : public Bitmap {
public:
  JsHeapBackedBitmap(
    const std::string& id,
    int32_t width,
    int32_t height,
    int32_t bytesPerPixel,
    std::unique_ptr<uint8_t[]>&& pixelBytes,
    JsBitmapPool* const parent,
    emscripten::val&& jsImageData)
    : id_(id), width_(width), height_(height), bytesPerPixel_(bytesPerPixel), wrappedBytes_(std::move(pixelBytes)), parent_(parent), jsImageData_(std::move(jsImageData)) {
      CHECK(wrappedBytes_ != nullptr) << "Failed to allocate bytes for JsHeapBackedBitmap";
  }

  ~JsHeapBackedBitmap() {
    if (parent_) {
        parent_->RemoveBitmap(id_);
    }
    JsOnDestroyBitmap(wrappedBytes_.release());
  }

  std::string_view id() const override {
    return id_;
  }

  int32_t width() const override {
    return width_;
  }

  int32_t height() const override {
    return height_;
  }

  int32_t bytesPerPixel() const override {
    return bytesPerPixel_;
  }

  uint8_t *bytes() override {
    return wrappedBytes_.get();
  }

  const uint8_t* bytes() const override {
    return wrappedBytes_.get();
  }

  void flush() override {
    onCapture(id_);
  }

  void onCapture(const std::string& slotIdStr) override {
    JsOnCaptureBitmap(slotIdStr.c_str(), jsImageData_.as_handle());
  }

private:
  const std::string id_;
  const int32_t width_;
  const int32_t height_;
  const int32_t bytesPerPixel_;
  std::unique_ptr<uint8_t[]> wrappedBytes_;

  JsBitmapPool* parent_ = nullptr;  // not owned, set by the pool when created.

  // Keep a reference to the JS ImageData object to signal onCapture event.
  emscripten::val jsImageData_;

  friend class JsBitmapPool;
};

std::unique_ptr<BitmapPool> CreateNewJsBitmapPool() {
    return std::make_unique<JsBitmapPool>();
}

} // namespace

//------------------------------ JsBitmapPool Implementation ----------------------------------
JsBitmapPool::JsBitmapPool() {
}

std::shared_ptr<Bitmap> JsBitmapPool::CreateNewBitmap(
      const std::string& resourceId,
      int32_t width, int32_t height, int32_t bytesPerPixel) {
    std::cout << "[JsBitmapPool] Creating new bitmap with id: " << resourceId << std::endl;
    const int32_t numBytes = width * height * bytesPerPixel;
    emscripten::val target = emscripten::val::object();
    target.set("width", width);
    target.set("height", height);
    target.set("numBytes", numBytes);

    emscripten::val jsObject = emscripten::val::take_ownership(JsOnCreateBitmap(target.as_handle()));
    if (jsObject.isNull()) {
        std::cout << "[JsBitmapPool] Failed to create bitmap for resource: " << resourceId << std::endl;
        return nullptr;
    }

  const intptr_t dataPtr = jsObject["dataPtr"].as<intptr_t>();
  emscripten::val imageData = jsObject["imageData"];

  std::cout << "[JsBitmapPool] Released staged bitmap info - width: " << width << ", height: " << height << ", bpp: " << bytesPerPixel << ", dataPtr: " << dataPtr << std::endl;

  uint8_t* const rawBytes = reinterpret_cast<uint8_t*>(dataPtr);
  std::unique_ptr<uint8_t[]> pixelData = std::unique_ptr<uint8_t[]>(rawBytes);



  auto bitmap = std::make_shared<JsHeapBackedBitmap>(
      resourceId, width, height, bytesPerPixel, std::move(pixelData), this, std::move(imageData));
  activeBitmaps_[std::string(bitmap->id())] = bitmap.get();
  std::cout << "[JsBitmapPool] returning bitmap (1) with id: " << bitmap->id() << std::endl;
  return bitmap;
}

std::shared_ptr<Bitmap> JsBitmapPool::ReleaseStagedBitmap(
        const std::string& reqSlotIdStr,
        const std::string& assetUri) {
  std::cout << "[JsBitmapPool] Releasing staged bitmap with URI: " << assetUri << " for slot: " << reqSlotIdStr << std::endl;
  emscripten::val jsObject = emscripten::val::take_ownership(JsReleaseStagedBitmap(reqSlotIdStr.c_str(), assetUri.c_str()));
  if (jsObject.isNull()) {
    std::cout << "[JsBitmapPool] No staged bitmap found for slot: " << reqSlotIdStr << " with asset URI: " << assetUri << std::endl;
    return nullptr;
  }
  const int32_t width = jsObject["width"].as<int32_t>();
  const int32_t height = jsObject["height"].as<int32_t>();
  const intptr_t dataPtr = jsObject["dataPtr"].as<intptr_t>();
  emscripten::val imageData = jsObject["imageData"];

  uint8_t* const rawBytes = reinterpret_cast<uint8_t*>(dataPtr);
  std::unique_ptr<uint8_t[]> pixelData = std::unique_ptr<uint8_t[]>(rawBytes);
  auto bitmap = std::make_shared<JsHeapBackedBitmap>(
      reqSlotIdStr, width, height, 4 /* bytesPerPixel */, std::move(pixelData), this, std::move(imageData));
  activeBitmaps_[std::string(bitmap->id())] = bitmap.get();
  return bitmap;
}

std::vector<const Bitmap*> JsBitmapPool::GetActiveBitmaps() const {
    std::vector<const Bitmap*> activeBitmaps;
    for (const auto& [_, bitmap] : activeBitmaps_) {
        activeBitmaps.push_back(bitmap);
    }
    return activeBitmaps;
}

void RegisterJsBitmapPool() {
    // This function can be called during initialization to register the JsBitmapPool implementation.
    BackendRegistry::Register<BitmapPool>(&CreateNewJsBitmapPool);
}
