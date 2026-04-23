#pragma once

#include <cstdint>

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

    virtual int32_t id() = 0;
    virtual int32_t width() = 0;
    virtual int32_t height() = 0;
    virtual int32_t byteSize() = 0;
    virtual uint8_t* bytes() = 0;
};