#pragma once

#include <optional>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plstate.h"
#include "ujcore/function/AttributeData.h"
#include "ujcore/function/AttributeDataType.h"
#include "ujcore/pipeline/PipelineSlot.h"

namespace ujcore {

class PipelineIONode {
public:
  explicit PipelineIONode(NodeId nodeId, bool isOutput);

  NodeId GetNodeId() const { return selfId_; }

  // Returns true if it is a graph output node, else it's a graph input node.
  // An input node will consume external data into the pipeline,
  // and an output node will produce data to be consumed by the
  // external environment.
  bool isOutputStage() const { return isOutput_; }

  absl::Status RunAsIO();

  absl::StatusOr<plstate::GraphRunOutput> GetRunResult() const;

private:
public:
    // Decodes from string to attribute (for graph input) or encodes from attribute to string
    // (for graph output).
    using ConvertFnPtr = std::variant<const AttributeEncodeFn*, const AttributeDecodeFn*>;

    absl::Status InternalRunAsInput();
    absl::Status InternalRunAsOutput();

  const NodeId selfId_;

    // Whether this is an graph-input node or graph-output node.
    const bool isOutput_;

    AttributeDataType dtype_ = AttributeDataType::kUnknown;

    // Input or output slot for graph IO.
    PipelineSlot slot_;

    // Used only for input node. This points to the encoded input data from the graph state,
    // which will be converted and stored in the `slot_` during pipeline execution.
    const std::optional<plstate::EncodedData>* encodedInput_ = nullptr;

    // Used only for output node. This holds the encoded output data after running the pipeline,
    // which will be returned to the external environment.
    // NOTE that for a graph input, the input data resides in the node state as it is
    // configured by the user. But for a graph output, the output data is generated during the
    // pipeline execution and can be different in different runs of the pipeline. So the output
    // data is buffered in this pipeline IO stage, and returned as pipeline's run result.
    std::optional<std::string> encodedOutput_;

    // Converts between graph's external input / output data (encoded), and internal attribute.
    // In which direction the conversion occurs depends on `isOutput_`, i.e., whether this is an
    // input or output node.
    ConvertFnPtr convertFnPtr_ = static_cast<const AttributeEncodeFn*>(nullptr);

    friend class PipelineBuilder;
};

}  // namespace ujcore
