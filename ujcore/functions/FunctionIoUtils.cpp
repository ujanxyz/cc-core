#include "ujcore/functions/FunctionIoUtils.hpp"

#include <algorithm>
#include <cstdint>

#include "absl/log/log.h"

namespace ujcore {
namespace {

using json = nlohmann::json;

// ---------- Param ----------
static json ParamToJson(const Param& p) {
    return {
        {"name", p.name},
        {"data_type", p.data_type}
    };
}

static json ParamVecToJson(const std::vector<Param>& vec) {
    json arr = json::array();
    for (const auto& p : vec) {
        arr.push_back(ParamToJson(p));
    }
    return arr;
}

// ---------- Merge helper ----------
static void Merge(json& dst, const json& src) {
    for (auto it = src.begin(); it != src.end(); ++it) {
        dst[it.key()] = it.value();
    }
}

// ---------- Extension serializers ----------

static json ToJson(const InputNodeExt& ext) {
    return {
        {"data_type", ext.data_type},
        {"accepts", ext.accepts}
    };
}

static json ToJson(const OutputNodeExt& ext) {
    return {
        {"data_type", ext.data_type},
        {"accepts", ext.accepts},
        {"emits", ext.emits}
    };
}

static json ToJson(const FunctionNodeExt& ext) {
    return {
        {"inputs", ParamVecToJson(ext.inputs)},
        {"outputs", ParamVecToJson(ext.outputs)}
    };
}

static json ToJson(const OperatorNodeExt& ext) {
    return {
        {"data_type", ext.data_type},
        {"inputs", ParamVecToJson(ext.inputs)}
    };
}

static json ToJson(const LambdaNodeExt& ext) {
    return {
        {"data_type", ext.data_type},
        {"arg_types", ext.arg_types},
        {"inputs", ParamVecToJson(ext.inputs)}
    };
}

// ------------------------------------------------------------
// Parse Helpers
// ------------------------------------------------------------

static bool ParseString(const json& j, const char* key, std::string& out) {
    if (!j.contains(key) || !j[key].is_string()) return false;
    out = j[key].get<std::string>();
    return true;
}

static bool ParseStringVec(const json& j, const char* key, std::vector<std::string>& out) {
    if (!j.contains(key) || !j[key].is_array()) return false;
    out.clear();
    for (const auto& v : j[key]) {
        if (!v.is_string()) return false;
        out.push_back(v.get<std::string>());
    }
    return true;
}

static bool ParseParam(const json& j, Param& p) {
    return ParseString(j, "name", p.name) &&
           ParseString(j, "data_type", p.data_type);
}

static bool ParseParamVec(const json& j, const char* key, std::vector<Param>& out) {
    if (!j.contains(key) || !j[key].is_array()) return false;

    out.clear();
    for (const auto& item : j[key]) {
        if (!item.is_object()) return false;
        Param p;
        if (!ParseParam(item, p)) return false;
        out.push_back(std::move(p));
    }
    return true;
}

}  // namespacee

std::string NodeFuncSpecToJsonStr(const NodeFunctionSpec& spec) {
    json j;

    // Common fields
    j["kind"] = spec.kind;
    j["label"] = spec.label;
    j["description"] = spec.description;

    json ext_json;

    if (spec.kind == "in") {
        ext_json = ToJson(std::any_cast<const InputNodeExt&>(spec.ext));
    } else if (spec.kind == "out") {
        ext_json = ToJson(std::any_cast<const OutputNodeExt&>(spec.ext));
    } else if (spec.kind == "fn") {
        ext_json = ToJson(std::any_cast<const FunctionNodeExt&>(spec.ext));
    } else if (spec.kind == "op") {
        ext_json = ToJson(std::any_cast<const OperatorNodeExt&>(spec.ext));
    } else if (spec.kind == "lam") {
        ext_json = ToJson(std::any_cast<const LambdaNodeExt&>(spec.ext));
    } else {
        LOG(FATAL) << "Unknown node kind: " << spec.kind;
    }

    Merge(j, ext_json);
    return j.dump();
}

bool ParseNodeFuncSpecFromJsonStr(std::string_view content, NodeFunctionSpec& spec) {
    json j;
    try {
      j = json::parse(content);
    } catch (...) {
        LOG(FATAL) << "Json parsing exception";
    }
    return ParseNodeFuncSpecFromJsonObj(j, spec);
}

bool ParseNodeFuncSpecFromJsonObj(const nlohmann::json& j, NodeFunctionSpec& spec) {
    if (!j.is_object()) return false;

    NodeFunctionSpec tmp;
    // -------- Common fields --------
    if (!ParseString(j, "kind", tmp.kind)) return false;
    if (!ParseString(j, "label", tmp.label)) return false;
    if (!ParseString(j, "description", tmp.description)) return false;

    // -------- Dispatch by kind --------
    if (tmp.kind == "in") {
        InputNodeExt ext;
        if (!ParseString(j, "data_type", ext.data_type)) return false;
        if (!ParseStringVec(j, "accepts", ext.accepts)) return false;
        tmp.ext = std::move(ext);
    }
    else if (tmp.kind == "out") {
        OutputNodeExt ext;
        if (!ParseString(j, "data_type", ext.data_type)) return false;
        if (!ParseStringVec(j, "accepts", ext.accepts)) return false;
        if (!ParseStringVec(j, "emits", ext.emits)) return false;
        tmp.ext = std::move(ext);
    }
    else if (tmp.kind == "fn") {
        FunctionNodeExt ext;
        if (!ParseParamVec(j, "inputs", ext.inputs)) return false;
        if (!ParseParamVec(j, "outputs", ext.outputs)) return false;
        tmp.ext = std::move(ext);
    }
    else if (tmp.kind == "op") {
        OperatorNodeExt ext;
        if (!ParseString(j, "data_type", ext.data_type)) return false;
        // inputs optional but must be array if present
        if (j.contains("inputs")) {
            if (!ParseParamVec(j, "inputs", ext.inputs)) return false;
        }
        tmp.ext = std::move(ext);
    }
    else if (tmp.kind == "lam") {
        LambdaNodeExt ext;
        if (!ParseString(j, "data_type", ext.data_type)) return false;
        if (!ParseStringVec(j, "arg_types", ext.arg_types)) return false;
        if (j.contains("inputs")) {
            if (!ParseParamVec(j, "inputs", ext.inputs)) return false;
        }
        tmp.ext = std::move(ext);
    }
    else {
        return false; // unknown kind
    }

    spec = std::move(tmp);
    return true;
}

}  // namespace ujcore