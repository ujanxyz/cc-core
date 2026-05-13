#include "ujcore/api_schemas/FlowApi.h"

#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/api_backends/SharedStore.h"

namespace ujcore {
namespace {

using GetFlowStatusResponse = FlowApi::GetFlowStatusResponse;
using BuildPipelineResponse = FlowApi::BuildPipelineResponse;

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

    BuildPipelineResponse buildPipelineImpl(const VoidType&) {
        auto assetInfosOr = store_.runner().RebuildFromState(store_.state());
        if (!assetInfosOr.ok()) {
            LOG(FATAL) << "Build pipeline error: " << assetInfosOr.status();
        }
        return BuildPipelineResponse {
            .assetInfos = std::move(assetInfosOr).value(),
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
    };
    cppschema::RegisterBackend<FlowApi, FlowApiBackend>(impl, ptrs);
}

}  // namespace ujcore
