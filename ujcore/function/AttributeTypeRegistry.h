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

    const AttributeEncodeFn* GetEncodFn(const std::string& dtypeName) const;
    const AttributeDecodeFn* GetDecodFn(const std::string& dtypeName) const;

    // Used in tests.
    void ClearRegistry();

private:
    AttributeTypeRegistry() = default;

    std::map<AttributeDataType /* dtype */, std::unique_ptr<TypeBuilder>> registry_;
};

}  // namespace ujcore