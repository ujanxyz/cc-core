# Design Incremental Pipeline Builder

This class `PipelineRunner` and its underlying struct `GraphPipeline`, manages the stages of execution in a graph based execution pipeline. These are build from the graph state, upstream libraries residing in `ujcore/graph/...`.

The next main objectives / plan with this library is to make the pipeline building process incremental and adapt to upstream graph events like addition / deletion of nodes and edges.

# Incremental Execution
Currently the class `PipelineBuilder` builds the full pipeline from scratch before running, but this approach might not scale as the graph size increases, and I also want to support incremental execution following the incremental building. Suppose node A is already executed and the output is available at its output slots. Now if user inserts a new node B via the graph UI and connects A to B by drawing an edge, B should immediately receive the output of A via the edge, and execute its function, without re-executing A or any prior nodes unless necessary.

We have kept the graph and pipeline separate. The pipeline is a linear view of the graph where the nodes are topologically sorted, and between the node stages there are slot transfer stages (to transport the output of the source node-slot to target node-slot). This is going to be a complex state management to keep the pipeline in sync with the source graph via incremental changes in response to graph edits.

# Objective
You are an experienced C++ engineer. Understand this requirement and come up with a concrete design, like data structures, high level apis and logic. Do not write any code yet.

Write you plan in `.github/incremental-pipeline-plan.md`
