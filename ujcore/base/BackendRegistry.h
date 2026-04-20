#pragma once

#include <memory>
#include <typeindex>

class BackendRegistry {
public:
    using VoidFactory = void*;

    template<class T>
    static void Register(std::unique_ptr<T> (*typedFactory)()) {
        VoidFactory factory = reinterpret_cast<VoidFactory>(typedFactory);
        RegisterForType(std::type_index(typeid(T)), factory);
    }
    
    template<class T>
    static std::unique_ptr<T> Create() {
        using TypedFactory = std::unique_ptr<T> (*)();
        VoidFactory voidPtr = CreateForType(std::type_index(typeid(T)));
        if (voidPtr == nullptr) {
            return nullptr;
        }
        const TypedFactory typedFactory = reinterpret_cast<TypedFactory>(voidPtr);
        return typedFactory();
    }

private:
    // Registers the factory for future lookup by type.
    static void RegisterForType(std::type_index typeIdx, VoidFactory factory);

    static VoidFactory CreateForType(std::type_index typeIdx);

    BackendRegistry() = delete;  // Prevent instantiation
};

