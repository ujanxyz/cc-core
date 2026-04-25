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
    
    virtual std::vector<const Bitmap*> GetActiveBitmaps() const = 0;
};

// Create a new pool using the registered implementation.
std::unique_ptr<BitmapPool> CreateNewBitmapPoolFromRegistry();