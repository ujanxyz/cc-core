#include "ujcore/functions/attributes/BitmapAttr.h"

#include <memory>

#include "nlohmann/json.hpp"
#include "ujcore/base/BitmapPool.h"
#include "ujcore/function/AttributeTypeRegistry.h"
#include "ujcore/function/EncodedAttr.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"
#include "ujcore/function/ParamAccessors.h"

namespace {

using json = ::nlohmann::json;
constexpr const char* kAssetUriFieldName = "assetUri";

// TODO: See ujcore/wasm/BitmapEncoding.cpp

}  // namespace

absl::StatusOr<std::string> GetBitmapAssetUriFromEncoded(const std::string& encoded) {
    // Decode the encoded string to get the asset URI.
    json jsonObj = json::parse(encoded);
    if (!jsonObj.is_object() || !jsonObj.contains(kAssetUriFieldName)) {
        return absl::InvalidArgumentError("Invalid encoded bitmap attribute: missing assetUri field");
    }
    const auto& ref = jsonObj[kAssetUriFieldName];
    if (!ref.is_string()) {
        return absl::InvalidArgumentError("Invalid encoded bitmap attribute: assetUri field is not a string");
    }
    return ref.get<std::string>();
}
