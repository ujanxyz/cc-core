#include "ujcore/base/BitmapPool.h"

#include <iostream>

#include "ujcore/base/BackendRegistry.h"

namespace {

class NativeBitmap final : public Bitmap {
public:
  NativeBitmap(int32_t width, int32_t height, int32_t bytesPerPixel)
    : width_(width), height_(height), bytesPerPixel_(bytesPerPixel) {
        const int32_t totalBytes = byteSize();
        ownedBytes_ = std::unique_ptr<uint8_t[]>(new uint8_t[totalBytes]);
  }

  ~NativeBitmap() {
    std::cout << "Destroying NativeBitmap with id: " << id() << std::endl;
  }

  int32_t id() override {
    return reinterpret_cast<intptr_t>(this);
  }

  int32_t width() override {
    return width_;
  }

  int32_t height() override {
    return height_;
  }

  int32_t byteSize() override {
    return width_ * height_ * bytesPerPixel_;
  }

  uint8_t *bytes() override {
    return ownedBytes_.get();
  }

private:
  const int32_t width_;
  const int32_t height_;
  const int32_t bytesPerPixel_;

  std::unique_ptr<uint8_t[]> ownedBytes_;
};

class NativeBitmapPool final : public BitmapPool {
public:
  std::shared_ptr<Bitmap> CreateNewBitmap(int32_t width, int32_t height,
                                               int32_t bytesPerPixel) override {
    auto bitmap = std::make_unique<NativeBitmap>(width, height, bytesPerPixel);
    return bitmap;
  }
};

}  // namespace

std::unique_ptr<BitmapPool> CreateNewBitmapPool() {
  auto pool = BackendRegistry::Create<BitmapPool>();
  if (pool == nullptr) {
      return std::make_unique<NativeBitmapPool>();
  } else {
      return std::unique_ptr<BitmapPool>(pool.release());
  }
}
