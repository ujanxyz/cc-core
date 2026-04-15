#include "ujcore/function/AttributeDataType.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"

using ujcore::FunctionRegistry;
using ujcore::FunctionSpec;
using ujcore::FunctionSpecBuilder;

class MyFunc final : public FunctionBase {
public:
    static inline const char* uri = "/test/myfn";

    static std::unique_ptr<FunctionSpec> spec() {
        return FunctionSpecBuilder(uri)
            .WithLabel("My Function")
            .WithDesc("This is my function.")
            .WithInputParam("p", "float2")
            .WithInputParam("dx", "float")
            .WithOutParam("fp", "float2")
            .Detach();
    }

    bool OnInit(const FunctionContext& ctx) override {
        return false;
    }

    bool OnRun(const FunctionContext& ctx) override {
        return false;
    }
};

static FunctionBase* create_FunctionBase_123() {
    return new MyFunc();
}
static std::unique_ptr<FunctionSpec> spec_FunctionBase_123() {
    return MyFunc::spec();
}

__attribute__((constructor)) void RegisterMyFunc() {
    FunctionRegistry::GetInstance().RegisterFunction(MyFunc::uri, spec_FunctionBase_123, create_FunctionBase_123, __FILE__);

}
