#pragma once

#include "ujcore/graph/GraphBuilder.h"
#include "ujcore/graph/GraphState.h"
#include "ujcore/graph/TopoSorter.h"
#include "ujcore/pipeline/PipelineRunner.h"

namespace ujcore {

// Shared state for graph and flow API backends.
class SharedStore final {
 public:
    static SharedStore& Instance();

    GraphState& state() { return state_; }
    TopoSorter& topoSorter() { return topoSorter_; }
    GraphBuilder& builder() { return builder_; }
    PipelineRunner& runner() { return runner_; }

 private:
    // No public constructor - this is a singleton class.
    SharedStore();

    // Disallow copy and assignment.
    SharedStore(const SharedStore&) = delete;
    SharedStore& operator=(const SharedStore&) = delete;

    GraphState state_;
    TopoSorter topoSorter_;
    GraphBuilder builder_;
    PipelineRunner runner_;
};

}  // namespace ujcore
