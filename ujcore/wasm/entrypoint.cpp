#include <string>


#include <emscripten/bind.h>
#include <emscripten/em_js.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/reflection.h"

#include "cppschema/wasm/js_api_bridge.h"
#include "ujcore/api_schemas/GraphEngineApi.h"
#include "ujcore/base/BuildInfo.h"

ABSL_FLAG(int32_t, test_flag, 10765, "test flag 05");

emscripten::val get_build_info() {
    using emval = emscripten::val;
    emval info = emval::object();
    const buildinfo::BuildInfo& build_info = buildinfo::GetSystemBuildInfo();
    info.set("timestamp", build_info.timestamp);
    info.set("hostname", build_info.hostname);
    info.set("user", build_info.user);
    info.set("revision", build_info.revision);
    info.set("status", build_info.status);
    return info;
}

// Explicitly parse Abseil flags from JavaScript arguments.
// As the emscripten module has no main function we emulate the values from JS as argc and argv.
int parse_abseil_flags(emscripten::val args) {
    if (!args.isArray()) {
        std::cerr << "[entrypoint] No arguments provided for Abseil flag parsing." << std::endl;
        return -1;
    }
    const std::vector<std::string> arrParts = emscripten::vecFromJSArray<std::string>(args);
    std::unique_ptr<char*[]> argVec(new char*[arrParts.size()]);
    for (size_t i = 0; i < arrParts.size(); ++i) {
        argVec[i] = const_cast<char*>(arrParts[i].c_str());
    }
    // Invoke Abseil's flag parsing.
    std::vector<char*> positionalArgs;
    std::vector<absl::UnrecognizedFlag> unrecognizedFlags;
    absl::ParseAbseilFlagsOnly(arrParts.size(), argVec.get(), positionalArgs, unrecognizedFlags);
    if (!unrecognizedFlags.empty()) {
        std::cerr << "[entrypoint] Warning: Unrecognized flags were provided, count:" << unrecognizedFlags.size() << std::endl;
    }
    auto allFlags = absl::GetAllFlags();
    return allFlags.size();
}

class HelloClass {
public:
    HelloClass(std::string message) : message(message) {}
    void sayHello() {
        std::cout << "HelloClass says: " << message << std::endl;
    }
private:
    std::string message;
};

EMSCRIPTEN_BINDINGS(EntrypointModule) {
    emscripten::function("getBuildInfo", &get_build_info);
    emscripten::function("parseAbseilFlags", &parse_abseil_flags);
    cppschema::jsbridge::CreateJsApiMethods<ujcore::GraphEngineApi>("GraphEngineApi");
}