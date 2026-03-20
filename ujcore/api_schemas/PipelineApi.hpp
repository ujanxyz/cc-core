#pragma once

#include "cppschema/apispec/api_framework.h"
#include "cppschema/common/types.h"

namespace ujcore {


struct PipelineUtilsApi {
    cppschema::ApiStub<VoidType, VoidType> clearGraph;

    DEFINE_API_VISITOR_FUNCTION(clearGraph);
};


}  // namespace ujcore
