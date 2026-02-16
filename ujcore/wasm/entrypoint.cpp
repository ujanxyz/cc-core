#include <iostream>
#include <string>

#include <emscripten/bind.h>
#include <emscripten/em_js.h>

#include "ujcore/base/build_info.hpp"

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

    emscripten::class_<HelloClass>("HelloClass")
        .constructor<std::string>()
        .function("sayHello", &HelloClass::sayHello);

    // cppschema::jsbridge::CreateJsApiMethods<graph::GraphApi>("GraphApi");
}