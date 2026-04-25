#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ujcore/base/BitmapPool.h"

// This is the BitmapPool implementation for the WASM environment.

class JsBitmapPool final : public BitmapPool {
public:
    JsBitmapPool();

    std::shared_ptr<Bitmap> CreateNewBitmap(
        const std::string& resourceId,
        int32_t width, int32_t height, int32_t bytesPerPixel) override;

    // Returns a list of active bitmaps that are currently in use.
    std::vector<const Bitmap*> GetActiveBitmaps() const override;

    void RemoveBitmap(const std::string& bitmapId) {
        activeBitmaps_.erase(bitmapId);
    }

private:
    // Tracks the bitmaps created but not deleted. Not owned.
    std::map<std::string, const Bitmap*> activeBitmaps_;
};

// Register the JsBitmapPool implementation so that it can be created by CreateNewBitmapPool().
void RegisterJsBitmapPool();
