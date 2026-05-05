#include "ujcore/base/BitmapPool.h"

#include <iostream>

#include "ujcore/base/BackendRegistry.h"

namespace {

class DummyBitmap final : public Bitmap {
public:
  DummyBitmap(int32_t width, int32_t height, int32_t bytesPerPixel)
    : width_(width), height_(height), bytesPerPixel_(bytesPerPixel) {
        const int32_t totalBytes = byteSize();
        ownedBytes_ = std::unique_ptr<uint8_t[]>(new uint8_t[totalBytes]);
  }

  ~DummyBitmap() {
    std::cout << "Destroying DummyBitmap with id: " << id() << std::endl;
  }

  std::string_view id() const override {
    return "n/a";
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
    return ownedBytes_.get();
  }

  const uint8_t* bytes() const override {
    return ownedBytes_.get();
  }

  void flush() override {
    // No-op for DummyBitmap
  }

  void onCapture(const std::string& slotIdStr) override {
    // No-op for DummyBitmap
  }

private:
  const int32_t width_;
  const int32_t height_;
  const int32_t bytesPerPixel_;

  std::unique_ptr<uint8_t[]> ownedBytes_;
};

// The fallback implementation, used if no other BitmapPool implementation is registered.
class FallbackBitmapPool final : public BitmapPool {
public:
  std::shared_ptr<Bitmap> CreateNewBitmap(
      const std::string& resourceId,
      int32_t width, int32_t height, int32_t bytesPerPixel) override {
    auto bitmap = std::make_unique<DummyBitmap>(width, height, bytesPerPixel);
    return bitmap;
  }

  std::shared_ptr<Bitmap> ReleaseStagedBitmap(
        const std::string& reqSlotIdStr,
        const std::string& assetUri) override {
    auto bitmap = std::make_unique<DummyBitmap>(1, 1, 4);
    return bitmap;
  }

  std::vector<const Bitmap*> GetActiveBitmaps() const override {
    // The fallback version does not implement this logic.
    return {};
  }
};

}  // namespace

std::unique_ptr<BitmapPool> CreateNewBitmapPoolFromRegistry() {
  auto pool = BackendRegistry::Create<BitmapPool>();
  if (pool == nullptr) {
      return std::make_unique<FallbackBitmapPool>();
  } else {
      return std::unique_ptr<BitmapPool>(pool.release());
  }
}
