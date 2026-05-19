#include "ujcore/api_schemas/FlowApi.h"

#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/api_backends/SharedStore.h"

namespace ujcore {
namespace {

using GetFlowStatusResponse = FlowApi::GetFlowStatusResponse;
using StepPipelineResponse = FlowApi::StepPipelineResponse;

} // namespace

class FlowApiBackend : public cppschema::ApiBackend<FlowApi> {
 public:
    FlowApiBackend() : store_(SharedStore::Instance()) {}

    GetFlowStatusResponse getFlowStatusImpl(const VoidType&) {
        return GetFlowStatusResponse{
            .status = "ready",
            .healthy = true,
        };
    }

    VoidType buildPipelineImpl(const VoidType&) {
        auto buildResultOr = store_.runner().RebuildFromState(store_.state());
        if (!buildResultOr.ok()) {
            LOG(FATAL) << "Build pipeline error: " << buildResultOr.status();
        }
        return VoidType {};
    }

    StepPipelineResponse stepPipelineImpl(const VoidType&) {
        auto stepResultOr = store_.runner().StepPipelineV2();
        if (!stepResultOr.ok()) {
            LOG(FATAL) << "Step pipeline error: " << stepResultOr.status();
        }
        return StepPipelineResponse {
            .stepResult = std::move(stepResultOr).value(),
        };
    }

 private:
    SharedStore& store_;
};

static __attribute__((constructor)) void RegisterFlowApiBackend() {
    auto* impl = new FlowApiBackend();
    FlowApi::ImplPtrs<FlowApiBackend> ptrs = {
        .getFlowStatus = &FlowApiBackend::getFlowStatusImpl,
        .buildPipeline = &FlowApiBackend::buildPipelineImpl,
        .stepPipeline = &FlowApiBackend::stepPipelineImpl,
    };
    cppschema::RegisterBackend<FlowApi, FlowApiBackend>(impl, ptrs);
}

}  // namespace ujcore
