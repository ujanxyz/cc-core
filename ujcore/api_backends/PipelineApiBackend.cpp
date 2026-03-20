#include "ujcore/api_schemas/PipelineApi.hpp"

#include <iostream>

#include "cppschema/apispec/api_registry.h"
#include "cppschema/backend/api_backend_bridge.h"
#include "ujcore/api_backends/IdGenerator.hpp"

namespace ujcore {

class PipelineApiBackend : public cppschema::ApiBackend<PipelineApi> {
 public:
    VoidType clearGraphImpl(const VoidType&) {
        for (int i = 0; i < 100; ++i) {
          const std::string newid = id_gen_.MakeNextId();
          std::cout << "Created id = " << newid << std::endl;
        }
        return VoidType{};
    }

 private:
  IdGenerator id_gen_;
};

static __attribute__((constructor)) void RegisterPipelineApiBackend() {
    auto* impl = new PipelineApiBackend();
    PipelineApi::ImplPtrs<PipelineApiBackend> ptrs = {
        .clearGraph = &PipelineApiBackend::clearGraphImpl,
    };
    cppschema::RegisterBackend<PipelineApi, PipelineApiBackend>(impl, ptrs);
}

}  // namespace ujcore
