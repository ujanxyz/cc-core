#pragma once

// This class does not include the definitions of the resources to avoid unnecessary coupling.
// It only provides an interface for function nodes to access resources. The function code
// should include the resource libraries (e.g. BitmapPool) if it needs to use them.
class BitmapPool;

class ResourceContext final {
public:
    ~ResourceContext() = default;

    BitmapPool* GetBitmapPool() {
        return bitmapPool_;
    }

    void setBitmapPool(BitmapPool* pool) {
        bitmapPool_ = pool;
    }

private:
    BitmapPool* bitmapPool_ = nullptr;
};
