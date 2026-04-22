#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/AttributeData.h"

namespace ujcore {

class AttributeTypeRegistry {
public:
    class TypeBuilder {
    public:
        virtual ~TypeBuilder() = default;
        virtual TypeBuilder& SetLabel(const std::string& label) = 0;
        virtual TypeBuilder& SetToEncodedFn(AttributeEncodeFn&& toEncoded) = 0;
        virtual TypeBuilder& SetFromEncodedFn(AttributeDecodeFn&& fromEncoded) = 0;
    };

    static AttributeTypeRegistry& GetInstance();

    TypeBuilder& MutableTypeBuilder(const std::string& dtypeName, const std::string& srcLoc);

    // Get a list of all registered type names, along with their UI labels.
    std::vector<std::tuple<std::string /* typeName */, std::string /* label */>> GetAllTypesAndLabels() const;

    const AttributeEncodeFn* GetEncodeFn(const std::string& dtypeName) const;
    const AttributeDecodeFn* GetDecodeFn(const std::string& dtypeName) const;

    // Used in tests.
    void ClearRegistry();

private:
    AttributeTypeRegistry() = default;

    std::map<AttributeDataType /* dtype */, std::unique_ptr<TypeBuilder>> registry_;
};

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FILE_LINE __FILE__ ":" TOSTRING(__LINE__)

}  // namespace ujcore