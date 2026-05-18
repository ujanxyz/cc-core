#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

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

    std::shared_ptr<Bitmap> ReleaseStagedBitmap(
        const std::string& reqSlotIdStr,
        const std::string& assetUri) override;

    std::shared_ptr<Bitmap> CreateAllocated(
        const std::string& resourceId,
        const IDimension& dimension,
        std::unique_ptr<uint8_t[]>&& pixelData) override;

    // Returns a list of active bitmaps that are currently in use.
    std::vector<const Bitmap*> GetActiveBitmaps() const override;

    void RemoveBitmap(const std::string& bitmapId) {
        activeBitmaps_.erase(bitmapId);
    }

private:
    // Tracks the bitmaps created but not deleted. Not owned.
    std::map<std::string, const Bitmap*> activeBitmaps_;
};

std::shared_ptr<Bitmap> CreateBitmapPreAllocated(emscripten::val jsImageData);

// Register the JsBitmapPool implementation so that it can be created by CreateNewBitmapPool().
void RegisterJsBitmapPool();
