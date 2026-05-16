#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/graph/GraphTypes.h"
#include "ujcore/graph/IdTypes.h"

// GraphTypes: Merged types for pipeline info and state.
// Fixed information and varying state for pipeline elements: node, edge, slot etc.

namespace ujcore::flow {

struct GraphOutputEntry {
    NodeId nodeId;
    std::string dtype;
    std::optional<grph::EncodedData> encodedData;

    DEFINE_STRUCT_VISITOR_FUNCTION(nodeId, dtype, encodedData);
};

struct AwaitEntry {
    NodeId nodeId;
    std::string channel;
    std::string workId;

    DEFINE_STRUCT_VISITOR_FUNCTION(nodeId, channel, workId);
};

struct FlowStepResult {
    enum class StatusEnum {
        PENDING = 0,
        SUCCESS,
        ERROR,
        PARTIAL,
    };

    StatusEnum status = StatusEnum::PENDING;
    std::vector<GraphOutputEntry> outputs;
    std::vector<AwaitEntry> awaitInfos;

    DEFINE_ENUM_CONVERSION_FUNCTION(StatusEnum, PENDING, SUCCESS, ERROR, PARTIAL);
    DEFINE_STRUCT_VISITOR_FUNCTION(status, outputs, awaitInfos);
};

} // namespace ujcore::flow
