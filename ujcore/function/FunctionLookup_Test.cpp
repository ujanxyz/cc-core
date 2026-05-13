#include "ujcore/function/FunctionLookup.h"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ujcore/function/FunctionBase.h"
#include "ujcore/function/FunctionRegistry.h"
#include "ujcore/function/FunctionSpec.h"

namespace {

class DummyFunction : public FunctionBase {
public:
    bool OnInit(FunctionContext& ctx) override {
        (void)ctx;
        return true;
    }

    ujfunc::FunctionReturn OnRun(FunctionContext& ctx) override {
        (void)ctx;
        return {.code = ujfunc::ReturnCode::DONE};
    }
};

FunctionBase* CreateDummyFunction() {
    return new DummyFunction();
}

std::unique_ptr<ujcore::FunctionSpec> MakeSpec(
    const std::string& uri,
    const std::string& label,
    const std::string& desc,
    const std::vector<ujcore::FuncParamSpec>& params) {
    auto spec = std::make_unique<ujcore::FunctionSpec>();
    spec->uri = uri;
    spec->label = label;
    spec->desc = desc;
    spec->params = params;
    return spec;
}

std::unique_ptr<ujcore::FunctionSpec> GetSpecMathAdd() {
    return MakeSpec(
        "/test/math/add",
        "Add",
        "Adds two float lists",
        {
            {"a", FuncParamAccess::kInput, AttributeDataType::kFloatList},
            {"b", FuncParamAccess::kInput, AttributeDataType::kFloatList},
            {"sum", FuncParamAccess::kOutput, AttributeDataType::kFloatList},
        });
}

std::unique_ptr<ujcore::FunctionSpec> GetSpecMathNormalize() {
    return MakeSpec(
        "/test/math/normalize",
        "Normalize",
        "Normalizes points",
        {
            {"points", FuncParamAccess::kInput, AttributeDataType::kPoints2D},
            {"normalized", FuncParamAccess::kOutput, AttributeDataType::kPoints2D},
        });
}

std::unique_ptr<ujcore::FunctionSpec> GetSpecImageBlur() {
    return MakeSpec(
        "/test/image/blur",
        "Blur",
        "Applies gaussian blur to bitmap",
        {
            {"bitmap", FuncParamAccess::kInOut, AttributeDataType::kBitmap},
            {"radius", FuncParamAccess::kInput, AttributeDataType::kFloatList},
        });
}

std::unique_ptr<ujcore::FunctionSpec> GetSpecImagePalette() {
    return MakeSpec(
        "/test/image/palette",
        "Palette",
        "Extracts dominant colors",
        {
            {"bitmap", FuncParamAccess::kInput, AttributeDataType::kBitmap},
            {"colors", FuncParamAccess::kOutput, AttributeDataType::kColors},
        });
}

void RegisterFunctionLookupTestFnsOnce() {
    static bool registered = false;
    if (registered) {
        return;
    }
    ujcore::FunctionRegistry& registry = ujcore::FunctionRegistry::GetInstance();
    registry.RegisterFunction("/test/math/add", &GetSpecMathAdd, &CreateDummyFunction, "FunctionLookup_Test:A");
    registry.RegisterFunction("/test/math/normalize", &GetSpecMathNormalize, &CreateDummyFunction, "FunctionLookup_Test:B");
    registry.RegisterFunction("/test/image/blur", &GetSpecImageBlur, &CreateDummyFunction, "FunctionLookup_Test:C");
    registry.RegisterFunction("/test/image/palette", &GetSpecImagePalette, &CreateDummyFunction, "FunctionLookup_Test:D");
    registered = true;
}

std::vector<std::string> ToUriList(const std::vector<ujcore::FunctionSpec>& specs) {
    std::vector<std::string> uris;
    uris.reserve(specs.size());
    for (const ujcore::FunctionSpec& spec : specs) {
        uris.push_back(spec.uri);
    }
    return uris;
}

TEST(FunctionLookupTest, FiltersByUriList) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> specs =
        ujcore::FunctionLookup()
            .WithUriList({"/test/math/add", "/test/image/palette", "/test/does/not/exist"})
            .Fetch();

    const std::vector<std::string> uris = ToUriList(specs);
    EXPECT_EQ(uris, (std::vector<std::string>{"/test/image/palette", "/test/math/add"}));
}

TEST(FunctionLookupTest, FiltersByPrefixAndText) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> specs =
        ujcore::FunctionLookup()
            .WithUriPrefix("/test/image")
            .WithTextContains("dominant")
            .Fetch();

    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0].uri, "/test/image/palette");
}

TEST(FunctionLookupTest, FiltersByParamDType) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> bitmapInputs =
        ujcore::FunctionLookup().WithInputDType(AttributeDataType::kBitmap).Fetch();
    const std::vector<std::string> bitmapInputUris = ToUriList(bitmapInputs);
    EXPECT_EQ(bitmapInputUris, (std::vector<std::string>{"/test/image/blur", "/test/image/palette"}));

    const std::vector<ujcore::FunctionSpec> colorOutputs =
        ujcore::FunctionLookup().WithOutputDType(AttributeDataType::kColors).Fetch();
    const std::vector<std::string> colorOutputUris = ToUriList(colorOutputs);
    EXPECT_EQ(colorOutputUris, (std::vector<std::string>{"/test/image/palette"}));
}

TEST(FunctionLookupTest, SupportsPagination) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> page =
        ujcore::FunctionLookup().WithUriPrefix("/test/").WithPagination(1, 2).Fetch();

    ASSERT_EQ(page.size(), 2u);
    const std::vector<std::string> uris = ToUriList(page);
    EXPECT_EQ(uris, (std::vector<std::string>{"/test/image/palette", "/test/math/add"}));
}

TEST(FunctionLookupTest, TextContainsMatchesLabelOrDesc) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> byLabel =
        ujcore::FunctionLookup().WithTextContains("Normalize").Fetch();
    ASSERT_EQ(byLabel.size(), 1u);
    EXPECT_EQ(byLabel[0].uri, "/test/math/normalize");

    const std::vector<ujcore::FunctionSpec> byDesc =
        ujcore::FunctionLookup().WithTextContains("gaussian").Fetch();
    ASSERT_EQ(byDesc.size(), 1u);
    EXPECT_EQ(byDesc[0].uri, "/test/image/blur");
}

TEST(FunctionLookupTest, FetchPreservesParams) {
    RegisterFunctionLookupTestFnsOnce();

    const std::vector<ujcore::FunctionSpec> specs =
        ujcore::FunctionLookup().WithUriList({"/test/math/add"}).Fetch();
    ASSERT_EQ(specs.size(), 1u);

    const ujcore::FunctionSpec& spec = specs[0];
    ASSERT_EQ(spec.params.size(), 3u);

    EXPECT_EQ(spec.params[0].name, "a");
    EXPECT_EQ(spec.params[0].access, FuncParamAccess::kInput);
    EXPECT_EQ(spec.params[0].dtype, AttributeDataType::kFloatList);

    EXPECT_EQ(spec.params[1].name, "b");
    EXPECT_EQ(spec.params[1].access, FuncParamAccess::kInput);

    EXPECT_EQ(spec.params[2].name, "sum");
    EXPECT_EQ(spec.params[2].access, FuncParamAccess::kOutput);
    EXPECT_EQ(spec.params[2].dtype, AttributeDataType::kFloatList);
}

}  // namespace
