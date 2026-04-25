#pragma once

#include <cstdint>
#include <string_view>

struct IDimesnion {
    int32_t width {0};
    int32_t height {0};
};

struct BytesData {
    int32_t size {0};
    uint8_t* bytes {nullptr};
};

class Bitmap {
public:
    virtual ~Bitmap() = default;

    virtual std::string_view id() const = 0;
    virtual int32_t width() const = 0;
    virtual int32_t height() const = 0;
    virtual int32_t bytesPerPixel() const = 0;
    virtual uint8_t* bytes() = 0;

    // Invoked after one stage has done modifying the content.
    virtual void flush() = 0;

    // Helper function to calculate the total byte size of the bitmap.
    int32_t byteSize() const {
        return width() * height() * bytesPerPixel();
    }
};