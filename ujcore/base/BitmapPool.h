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
        IDimension dimension,
        int32_t bytesPerPixel) = 0;

    virtual std::shared_ptr<Bitmap> CreateAllocated(
        const std::string& resourceId,
        const IDimension& dimension,
        std::unique_ptr<uint8_t[]>&& pixelData) = 0;
};

// Create a new pool using the registered implementation.
std::unique_ptr<BitmapPool> CreateNewBitmapPoolFromRegistry();