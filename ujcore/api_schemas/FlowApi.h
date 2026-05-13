#pragma once

#include <string>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/graph/AssetInfo.h"

namespace ujcore {

struct FlowApi {
    struct GetFlowStatusResponse {
        std::string status;
        bool healthy = true;

        DEFINE_STRUCT_VISITOR_FUNCTION(status, healthy);
    };

    // API: buildPipeline
    struct BuildPipelineResponse {
        std::vector<AssetInfo> assetInfos;
        DEFINE_STRUCT_VISITOR_FUNCTION(assetInfos);
    };

    cppschema::ApiStub<VoidType, GetFlowStatusResponse> getFlowStatus;
    cppschema::ApiStub<VoidType, BuildPipelineResponse> buildPipeline;

    DEFINE_API_VISITOR_FUNCTION(getFlowStatus, buildPipeline);
};

}  // namespace ujcore
