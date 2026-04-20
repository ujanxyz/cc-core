#pragma once

#include <memory>

#include "ujcore/base/Bitmap.h"

class BitmapPool {
public:
    virtual ~BitmapPool() = default;

    virtual std::shared_ptr<Bitmap> CreateNewBitmap(
        int32_t width, int32_t height, int32_t bytesPerPixel) = 0;
};

// Get an instance of BitmapPool implementation based on the registered impl.
BitmapPool& GetBitmapPool();
