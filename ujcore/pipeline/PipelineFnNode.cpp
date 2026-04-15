#include "ujcore/pipeline/PipelineFnNode.h"

#include "absl/log/log.h"

/* static */
std::unique_ptr<PipelineFnNode>
PipelineFnNode::Create(const FunctionSpec& funcSpec, std::unique_ptr<FunctionBase> funcInstance) {
    auto node = std::make_unique<PipelineFnNode>();
    node->funcInstance_ = std::move(funcInstance);
    node->functionCtx_ = std::make_unique<FunctionContext>(node.get());

    node->slots_["x"] = SlotStorage {
            .access = SlotStorage::AccessKind::kInput,
            .attribute = {
                .dtype = AttributeDataType::kFloat,
            },
        };


    if (!node->funcInstance_->OnInit(*node->functionCtx_)) {
        LOG(ERROR) << "Failed to initialize function instance for node.";
        return nullptr;
    }
    return node;
}
