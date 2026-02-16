#include <emscripten/bind.h>
#include <emscripten/em_js.h>

#include <iostream>
#include <string>

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
    emscripten::class_<HelloClass>("HelloClass")
        .constructor<std::string>()
        .function("sayHello", &HelloClass::sayHello);

    // cppschema::jsbridge::CreateJsApiMethods<graph::GraphApi>("GraphApi");
}