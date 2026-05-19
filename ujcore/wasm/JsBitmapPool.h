#pragma once

#include <map>
#include <memory>
#include <string>

#include "emscripten/val.h"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/base/IDimension.h"

// This is the BitmapPool implementation for the WASM environment.

class JsBitmapPool final : public BitmapPool {
public:
    JsBitmapPool();

    std::shared_ptr<Bitmap> CreateNewBitmap(
        const std::string& resourceId,
        IDimension dimension,
        int32_t bytesPerPixel) override;

    std::shared_ptr<Bitmap> CreateAllocated(
        const std::string& resourceId,
        const IDimension& dimension,
        std::unique_ptr<uint8_t[]>&& pixelData) override;

    void RemoveBitmap(const std::string& bitmapId) {
        activeBitmaps_.erase(bitmapId);
    }

private:
    // Tracks the bitmaps created but not deleted. Not owned.
    std::map<std::string, const Bitmap*> activeBitmaps_;
};

// Register the JsBitmapPool implementation so that it can be created by CreateNewBitmapPool().
void RegisterJsBitmapPool();
