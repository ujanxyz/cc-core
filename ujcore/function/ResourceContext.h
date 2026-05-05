#pragma once

#include <string>

// This class does not include the definitions of the resources to avoid unnecessary coupling.
// It only provides an interface for function nodes to access resources. The function code
// should include the resource libraries (e.g. BitmapPool) if it needs to use them.
class BitmapPool;

class ResourceContext final {
public:
    ~ResourceContext() = default;

    void SetSlotIdStr(const std::string& slotIdStr) {
        slotIdStr_ = slotIdStr;
    }

    const std::string& GetSlotIdStr() const {
        return slotIdStr_;
    }

    BitmapPool* GetBitmapPool() {
        return bitmapPool_;
    }

    void setBitmapPool(BitmapPool* pool) {
        bitmapPool_ = pool;
    }

private:
    std::string slotIdStr_;  // For tracking which slot is using the resource.
    BitmapPool* bitmapPool_ = nullptr;
};
