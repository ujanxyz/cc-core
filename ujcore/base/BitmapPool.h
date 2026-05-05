#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ujcore/base/Bitmap.h"

class BitmapPool {
public:
    virtual ~BitmapPool() = default;

    virtual std::shared_ptr<Bitmap> CreateNewBitmap(
        const std::string& resourceId,
        int32_t width, int32_t height, int32_t bytesPerPixel) = 0;

    // Extract the staged bitmap for the given asset URI and return it,
    // and also remove it from the pool's staging area.
    // `reqSlotIdStr`: The requesting slot's spec, for tracking who used it.
    // `assetUri`: The URI of the asset to release.
    virtual std::shared_ptr<Bitmap> ReleaseStagedBitmap(
        const std::string& reqSlotIdStr,
        const std::string& assetUri) = 0;
    
    virtual std::vector<const Bitmap*> GetActiveBitmaps() const = 0;
};

// Create a new pool using the registered implementation.
std::unique_ptr<BitmapPool> CreateNewBitmapPoolFromRegistry();