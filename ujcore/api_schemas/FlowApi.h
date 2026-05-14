#pragma once

#include <vector>
#include <string>

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/pipeline/FlowTypes.h"
#include "ujcore/graph/AssetInfo.h"

namespace ujcore {

struct FlowApi {
    // API: getFlowStatus
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

    // API: stepPipeline
    struct StepPipelineResponse {
        flow::FlowStepResult stepResult;

        DEFINE_STRUCT_VISITOR_FUNCTION(stepResult);
    };

    cppschema::ApiStub<VoidType, GetFlowStatusResponse> getFlowStatus;
    cppschema::ApiStub<VoidType, BuildPipelineResponse> buildPipeline;
    cppschema::ApiStub<VoidType, StepPipelineResponse> stepPipeline;
    
    DEFINE_API_VISITOR_FUNCTION(getFlowStatus, buildPipeline, stepPipeline);
};

}  // namespace ujcore
