#include "ujcore/wasm/JsBitmapPool.h"

#include <emscripten/em_asm.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>

#include "absl/log/check.h"
#include "ujcore/base/BackendRegistry.h"

namespace {

EM_JS(uint8_t*, JsOnCreateBitmap, (emscripten::EM_VAL handle, uint8_t* pixelsData), {
    const target = Emval.toValue(handle);
    const {id, width, height, numBytes} = target;
    const ptr = Module._malloc(numBytes);
    const uint8arr = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, numBytes);
    globalThis.pipelineEvents.dispatchEvent(new CustomEvent("BITMAP_CREATED", { detail: { ... target, uint8arr } }));
    return ptr;
});

EM_JS(void, JsOnDestroyBitmap, (const char* idStr, uint8_t* pixelsData), {
    globalThis.pipelineEvents.dispatchEvent(new CustomEvent("BITMAP_DELETED", { detail: { id: UTF8ToString(idStr) } }));
    Module._free(pixelsData);
});

EM_JS(void, JsOnFlushBitmap, (const char* idStr), {
    globalThis.pipelineEvents.dispatchEvent(new CustomEvent("BITMAP_FLUSHED", { detail: { id: UTF8ToString(idStr) } }));
});

class JsHeapBackedBitmap final : public Bitmap {
public:
  JsHeapBackedBitmap(const std::string& id, int32_t width, int32_t height, int32_t bytesPerPixel, JsBitmapPool* const parent)
    : id_(id), width_(width), height_(height), bytesPerPixel_(bytesPerPixel), parent_(parent) {
        emscripten::val target = emscripten::val::object();
        target.set("id", id_);
        target.set("width", width);
        target.set("height", height); 
        target.set("numBytes", byteSize());
        uint8_t* const rawBytes = JsOnCreateBitmap(target.as_handle(), wrappedBytes_.get());
        wrappedBytes_ = std::unique_ptr<uint8_t[]>(rawBytes);
        CHECK(wrappedBytes_ != nullptr) << "Failed to allocate bytes for JsHeapBackedBitmap";
  }

  ~JsHeapBackedBitmap() {
    if (parent_) {
        parent_->RemoveBitmap(id_);
    }
    JsOnDestroyBitmap(id_.c_str(), wrappedBytes_.release());
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

  void flush() override {
    JsOnFlushBitmap(id_.c_str());
  }

private:
  const std::string id_;
  const int32_t width_;
  const int32_t height_;
  const int32_t bytesPerPixel_;

  JsBitmapPool* parent_ = nullptr;  // not owned, set by the pool when created.

  std::unique_ptr<uint8_t[]> wrappedBytes_;
};

std::unique_ptr<BitmapPool> CreateNewJsBitmapPool() {
    return std::make_unique<JsBitmapPool>();
}

} // namespace

JsBitmapPool::JsBitmapPool() {
}

std::shared_ptr<Bitmap> JsBitmapPool::CreateNewBitmap(
      const std::string& resourceId,
      int32_t width, int32_t height, int32_t bytesPerPixel) {
    auto bitmap = std::make_unique<JsHeapBackedBitmap>(resourceId, width, height, bytesPerPixel, this);
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
