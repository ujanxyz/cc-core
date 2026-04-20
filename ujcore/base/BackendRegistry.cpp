#include "ujcore/base/BackendRegistry.h"

#include <iostream>
#include <map>
#include <typeindex>

namespace {

using VoidFactory = BackendRegistry::VoidFactory;

class RegistryStore {
public:
    static RegistryStore& GetInstance() {
        static RegistryStore instance;
        return instance;
    }

    void Register(std::type_index type, VoidFactory factory) {
        if (registryMap.count(type) > 0) {
            std::cerr << "Warning: Factory for type " << type.name() << " is already registered." << std::endl;
        }
        registryMap[type] = factory;
    }

    VoidFactory Lookup(std::type_index type) const {
        auto it = registryMap.find(type);
        if (it != registryMap.end()) {
            return it->second;
        }
        return nullptr;  // Or throw an exception if preferred
    }

private:
    // This is a simple in-memory store for registered factories.
    // In a real implementation, this might be more complex and thread-safe.
    std::map<std::type_index, VoidFactory> registryMap;
};

}  // namespace

/* static */
void BackendRegistry::RegisterForType(std::type_index typeIdx, VoidFactory factory) {
    RegistryStore::GetInstance().Register(typeIdx, factory);
}

/* static */
VoidFactory BackendRegistry::CreateForType(std::type_index typeIdx) {
    const VoidFactory voidPtr = RegistryStore::GetInstance().Lookup(typeIdx);
    return voidPtr;
}

