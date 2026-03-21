
#include "ujcore/functions/FunctionIoUtils.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"
#include "nlohmann/json.hpp"

namespace ujcore {
namespace {

using json = nlohmann::json;

static json Parse(const std::string& s) {
    return json::parse(s);
}

// ✅ Unparse: Input Function
TEST(FunctionIoUtilsTest, Unparse_InputNode) {
    NodeFunctionSpec spec;
    spec.kind = "in";
    spec.label = "Input";
    spec.description = "Test input";

    InputNodeExt ext;
    ext.data_type = "float";
    ext.accepts = {"number", "slider"};

    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    EXPECT_EQ(j["kind"], "in");
    EXPECT_EQ(j["label"], "Input");
    EXPECT_EQ(j["description"], "Test input");

    EXPECT_EQ(j["data_type"], "float");
    EXPECT_EQ(j["accepts"].size(), 2);
}

// ✅ Unparse: Output Function
TEST(FunctionIoUtilsTest, Unparse_OutputNode) {
    NodeFunctionSpec spec;
    spec.kind = "out";
    spec.label = "Output";
    spec.description = "Test output";

    OutputNodeExt ext;
    ext.data_type = "image";
    ext.accepts = {"rgba"};
    ext.emits = {"png"};

    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    EXPECT_EQ(j["kind"], "out");
    EXPECT_EQ(j["data_type"], "image");
    EXPECT_EQ(j["accepts"][0], "rgba");
    EXPECT_EQ(j["emits"][0], "png");
}

// ✅ Unparse: Operator Function
TEST(FunctionIoUtilsTest, Unparse_FunctionNode) {
    NodeFunctionSpec spec;
    spec.kind = "fn";
    spec.label = "Add";
    spec.description = "Adds numbers";

    FunctionNodeExt ext;
    ext.inputs = {
        {"a", "float"},
        {"b", "float"}
    };
    ext.outputs = {
        {"result", "float"}
    };

    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    ASSERT_TRUE(j.contains("inputs"));
    ASSERT_TRUE(j.contains("outputs"));

    EXPECT_EQ(j["inputs"].size(), 2);
    EXPECT_EQ(j["outputs"].size(), 1);

    EXPECT_EQ(j["inputs"][0]["name"], "a");
    EXPECT_EQ(j["inputs"][0]["data_type"], "float");

    EXPECT_EQ(j["outputs"][0]["name"], "result");
}

// ✅ Unparse: Operator Node
TEST(FunctionIoUtilsTest, Unparse_OperatorNode) {
    NodeFunctionSpec spec;
    spec.kind = "op";
    spec.label = "Scale";
    spec.description = "Scales value";

    OperatorNodeExt ext;
    ext.data_type = "float";
    ext.inputs = {
        {"factor", "float"}
    };

    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    EXPECT_EQ(j["data_type"], "float");
    ASSERT_EQ(j["inputs"].size(), 1);
    EXPECT_EQ(j["inputs"][0]["name"], "factor");
}

// ✅ Unparse: Lambda Node
TEST(FunctionIoUtilsTest, Unparse_LambdaNode) {
    NodeFunctionSpec spec;
    spec.kind = "lam";
    spec.label = "Noise";
    spec.description = "Generates noise";

    LambdaNodeExt ext;
    ext.data_type = "float";
    ext.arg_types = {"vec2"};
    ext.inputs = {
        {"seed", "int"}
    };

    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    EXPECT_EQ(j["data_type"], "float");
    EXPECT_EQ(j["arg_types"][0], "vec2");

    ASSERT_EQ(j["inputs"].size(), 1);
    EXPECT_EQ(j["inputs"][0]["name"], "seed");
}

// ❌ Unparse: Invalid Kind Test
TEST(FunctionIoUtilsTest, Unparse_InvalidKindThrows) {
    NodeFunctionSpec spec;
    spec.kind = "unknown";
    spec.label = "Bad";
    spec.description = "Invalid";

    spec.ext = InputNodeExt{};  // doesn't matter
    EXPECT_DEATH(NodeFuncSpecToJsonStr(spec), "Unknown node kind");
}

// ❌ Unparse: Missing Fields Sanity Test (Ensures no unexpected fields leak)
TEST(FunctionIoUtilsTest, Unparse_NoExtFieldPresent) {
    NodeFunctionSpec spec;
    spec.kind = "fn";
    spec.label = "Add";
    spec.description = "Adds";

    FunctionNodeExt ext;
    spec.ext = ext;

    auto j = Parse(NodeFuncSpecToJsonStr(spec));

    EXPECT_FALSE(j.contains("ext"));  // important for flattened design
}

// ✅ Parse: Input node
TEST(FunctionIoUtilsTest, Parse_InputNode) {
    std::string json = R"({
        "kind": "in",
        "label": "Input",
        "description": "Test input",
        "data_type": "float",
        "accepts": ["number", "slider"]
    })";
    NodeFunctionSpec spec;
    ASSERT_TRUE(ParseNodeFuncSpecFromJsonStr(json, spec));

    EXPECT_EQ(spec.kind, "in");
    EXPECT_EQ(spec.label, "Input");

    auto* ext = std::any_cast<InputNodeExt>(&spec.ext);
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->data_type, "float");
    EXPECT_EQ(ext->accepts.size(), 2);
}

// ✅ Parse: Function node
TEST(FunctionIoUtilsTest, Parse_FunctionNode) {
    std::string json = R"({
        "kind": "fn",
        "label": "Add",
        "description": "Adds numbers",
        "inputs": [
            { "name": "a", "data_type": "float" },
            { "name": "b", "data_type": "float" }
        ],
        "outputs": [
            { "name": "result", "data_type": "float" }
        ]
    })";
    NodeFunctionSpec spec;
    ASSERT_TRUE(ParseNodeFuncSpecFromJsonStr(json, spec));

    auto* ext = std::any_cast<FunctionNodeExt>(&spec.ext);
    ASSERT_NE(ext, nullptr);

    EXPECT_EQ(ext->inputs.size(), 2);
    EXPECT_EQ(ext->outputs.size(), 1);
    EXPECT_EQ(ext->inputs[0].name, "a");
}

// ✅ Parse: Operator node with optional inputs
TEST(FunctionIoUtilsTest, Parse_OperatorNodeWithoutInputs) {
    std::string json = R"({
        "kind": "op",
        "label": "Scale",
        "description": "Scale value",
        "data_type": "float"
    })";
    NodeFunctionSpec spec;
    ASSERT_TRUE(ParseNodeFuncSpecFromJsonStr(json, spec));

    auto* ext = std::any_cast<OperatorNodeExt>(&spec.ext);
    ASSERT_NE(ext, nullptr);

    EXPECT_TRUE(ext->inputs.empty());
}

// ✅ Parse: Lambda node
TEST(FunctionIoUtilsTest, Parse_LambdaNodeSuccess) {
    std::string json = R"({
        "kind": "lam",
        "label": "Noise",
        "description": "Generates noise",
        "data_type": "float",
        "arg_types": ["vec2"],
        "inputs": [
            { "name": "seed", "data_type": "int" }
        ]
    })";
    NodeFunctionSpec spec;
    ASSERT_TRUE(ParseNodeFuncSpecFromJsonStr(json, spec));

    auto* ext = std::any_cast<LambdaNodeExt>(&spec.ext);
    ASSERT_NE(ext, nullptr);

    EXPECT_EQ(ext->arg_types.size(), 1);
    EXPECT_EQ(ext->inputs.size(), 1);
}

// ❌ Parse: Missing required field
TEST(FunctionIoUtilsTest, ParseFailsMissingKind) {
    std::string json = R"({
        "label": "Bad",
        "description": "Missing kind"
    })";
    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Missing extension field
TEST(FunctionIoUtilsTest, ParseFailsMissingDataType) {
    std::string json = R"({
        "kind": "in",
        "label": "Input",
        "description": "Bad input",
        "accepts": ["x"]
    })";
    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Wrong type, accepts should be array
TEST(FunctionIoUtilsTest, ParseFailsWrongTypeArray) {
    std::string json = R"({
        "kind": "in",
        "label": "Input",
        "description": "Bad input",
        "data_type": "float",
        "accepts": "not-an-array"
    })";

    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Wrong type inside array
TEST(FunctionIoUtilsTest, ParseFailsInvalidArrayElement) {
    std::string json = R"({
        "kind": "in",
        "label": "Input",
        "description": "Bad input",
        "data_type": "float",
        "accepts": ["ok", 123]
    })";
    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Invalid param object
TEST(FunctionIoUtilsTest, ParseFailsInvalidParamObject) {
    std::string json = R"({
        "kind": "fn",
        "label": "Add",
        "description": "Bad fn",
        "inputs": [
            { "name": "a" }
        ],
        "outputs": []
    })";
    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Unknown kind
TEST(FunctionIoUtilsTest, ParseFailsUnknownKind) {
    std::string json = R"({
        "kind": "weird",
        "label": "Bad",
        "description": "Unknown kind"
    })";

    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

// ❌ Parse: Invalid JSON
TEST(FunctionIoUtilsTest, ParseFailsInvalidJson) {
    std::string json = R"({
        "kind": "in",
        "label": "Oops",
        "description": "Broken",
        "data_type": "float",
        "accepts": [1, 2]
    )"; // missing closing brace

    NodeFunctionSpec spec;
    EXPECT_DEATH(ParseNodeFuncSpecFromJsonStr(json, spec), "Json parsing exception");
}

// ❌ Parse: Inputs not array
TEST(FunctionIoUtilsTest, ParseFailsInputsNotArray) {
    std::string json = R"({
        "kind": "fn",
        "label": "Add",
        "description": "Bad fn",
        "inputs": "oops",
        "outputs": []
    })";

    NodeFunctionSpec spec;
    EXPECT_FALSE(ParseNodeFuncSpecFromJsonStr(json, spec));
}

}  // namespace
}  // namespace ujcore