# Incremental Pipeline Plan

## 1. Context

`PipelineRunner` currently rebuilds a full `GraphPipeline` from `GraphState` and then executes.

Goal: support incremental updates from graph edits (`add/remove node`, `add/remove edge`, manual data updates) without full rebuild, and support incremental execution so unaffected upstream work is not re-run.

This plan focuses on architecture, data structures, APIs, and logic only (no code).

## 2. Design Goals

- Keep graph (`GraphState`) and execution view (`GraphPipeline`) separate.
- Apply edit deltas to pipeline in near O(changed-subgraph) time.
- Re-execute only dirty/downstream-affected nodes.
- Preserve correctness under topological constraints.
- Keep behavior deterministic and debuggable.
- Integrate with existing `FunctionReturn` semantics (`DONE`, `NO_DATA`, `AWAIT`, `ERROR`).

## 3. Non-Goals

- Distributed execution or multi-machine scheduling.
- Parallel execution in first version.
- Persistent checkpointing beyond in-memory runtime state.

## 4. Core Invariants

- Pipeline steps are always a valid topological linearization of active graph edges.
- Every edge transfer step appears before target node run step.
- Node stage has stable identity by `NodeId` even if execution step indices shift.
- Dirty propagation is monotonic within one update transaction.
- No execution starts while an update transaction is mid-apply.

## 5. New Runtime Data Structures

## 5.1 PipelineIndex (inside `GraphPipeline`)

Purpose: fast lookup and local rewrites.

Fields:

- `std::map<NodeId, size_t> nodeStepIndex`
- `std::map<EdgeId, size_t> edgeStepIndex`
- `std::map<NodeId, std::set<NodeId>> execDownstream`
- `std::map<NodeId, std::set<NodeId>> execUpstream`

## 5.2 NodeExecState (inside `GraphPipeline`)

Purpose: track incremental run eligibility.

Fields:

- `enum class Dirtiness { CLEAN, DIRTY_INPUT, DIRTY_STRUCTURE, DIRTY_MANUAL_OVERRIDE }`
- `bool hasExecuted`
- `bool hasOutput`
- `bool awaiting`
- `std::optional<ujfunc::AwaitInfo> awaitInfo`
- `std::optional<absl::Status> lastError`
- `uint64_t version`

Map:

- `std::map<NodeId, NodeExecState> nodeExecState`

## 5.3 UpdateTxn

Purpose: batch graph edit events and apply atomically.

Fields:

- `std::vector<NodeId> addedNodes`
- `std::vector<NodeId> removedNodes`
- `std::vector<EdgeId> addedEdges`
- `std::vector<EdgeId> removedEdges`
- `std::vector<SlotId> manualDataChangedSlots`
- `std::set<NodeId> explicitDirtyRoots`

## 6. High-Level API Additions

## 6.1 PipelineBuilder

- `absl::StatusOr<GraphPipeline> BuildFull(const GraphState& state)`
- `absl::Status ApplyIncremental(const GraphState& state, const UpdateTxn& txn, GraphPipeline& pipeline)`
- `absl::Status Reindex(GraphPipeline& pipeline)`
- `absl::Status ValidatePipeline(const GraphState& state, const GraphPipeline& pipeline)`

## 6.2 PipelineRunner

- `absl::StatusOr<std::vector<AssetInfo>> RebuildFromState(const GraphState& state)` (existing full rebuild path)
- `absl::Status ApplyGraphUpdate(const GraphState& state, const UpdateTxn& txn)`
- `absl::StatusOr<std::vector<grph::GraphRunOutput>> RunIncremental(const std::set<NodeId>& roots)`
- `absl::Status MarkDirty(const std::set<NodeId>& roots, NodeExecState::Dirtiness reason)`
- `absl::StatusOr<std::set<NodeId>> ComputeAffected(const std::set<NodeId>& roots) const`

## 6.3 GraphApi / FlowApi Integration

Graph API should emit edit transactions while mutating graph:

- `createNode/createIONode/addEdges/deleteElements/setEncodedData` produce `UpdateTxn` entries.
- Backend calls `store_.runner().ApplyGraphUpdate(store_.state(), txn)`.

Flow API can observe status:

- Add future endpoint(s) to expose runner health/await queue/dirty count.

## 7. Incremental Update Logic

## 7.1 Add Node

- Insert new node run stage based on topological position from `TopoSorter` order.
- Initialize `NodeExecState` as dirty (`DIRTY_STRUCTURE`, `hasExecuted=false`).
- No upstream node is dirtied.

## 7.2 Add Edge

- Insert transfer step before target node run step.
- Mark target node and its downstream as `DIRTY_INPUT`.
- If source already has output, allow immediate transfer on next incremental run.

## 7.3 Remove Edge

- Remove transfer step.
- Mark target node and downstream `DIRTY_INPUT`.
- Clear target slot derived data if no remaining source for that slot.

## 7.4 Remove Node

- Remove node run stage and incident transfer stages.
- Remove `NodeExecState` entry.
- Mark downstream nodes `DIRTY_STRUCTURE` because required inputs may vanish.

## 7.5 Manual Slot Data Change

- Mark owning node dirty (`DIRTY_MANUAL_OVERRIDE`), then downstream `DIRTY_INPUT`.

## 8. Incremental Execution Logic

Given dirty roots:

1. Compute affected closure through `execDownstream`.
2. Iterate affected nodes in topological order only.
3. For each node:
   - Run inbound transfer steps first.
   - Invoke function node.
   - Handle `FunctionReturn`:
     - `DONE`: mark clean and propagate output availability.
     - `AWAIT`: keep node awaiting; stop downstream of this branch until resumed.
     - `NO_DATA`: mark node waiting; do not fail whole run; keep downstream blocked.
     - `ERROR`: mark failed; return non-ok status from incremental run.
4. Produce outputs only for graph OUT nodes whose values changed this run.

## 9. Await/Resume Model

- Maintain `awaitingNodes: std::map<NodeId, ujfunc::AwaitInfo>`.
- On resume event, mark resumed node as dirty root and run incremental from it.
- Ensure token validation to avoid stale resume events.

## 10. Consistency and Safety

- Guard updates with a runner-level mutex (single writer).
- Reject execution if pipeline validation fails.
- Provide fallback path: if incremental apply detects structural inconsistency, trigger full rebuild.

Fallback trigger examples:

- Reindex mismatch.
- Topological order disagreement.
- Missing stage for a graph element.

## 11. Observability

Expose debug snapshots:

- `dirty node count`
- `awaiting node count`
- `last incremental apply duration`
- `last incremental run duration`
- `full rebuild fallback count`

## 12. Rollout Strategy

## Phase 1

- Add data structures and APIs.
- Keep full rebuild as default behavior.

## Phase 2

- Implement incremental apply for edge and manual-data updates.
- Incremental run enabled behind flag.

## Phase 3

- Add node/edge deletion incremental paths.
- Add await/resume robust handling.

## Phase 4

- Enable incremental mode by default.
- Keep automatic fallback to full rebuild.

## 13. Test Plan

- Unit tests for each update operation (`add/remove node`, `add/remove edge`, manual override).
- Property-style tests comparing incremental result vs full rebuild result for random edit sequences.
- Regression tests for `AWAIT`, `NO_DATA`, and error propagation.
- Performance tests: large DAG edit bursts and incremental run latency.

## 14. Open Questions

- Should `NO_DATA` fail Graph API call or be represented as partial-progress status?
- Should awaiting nodes block unrelated independent branches?
- Do we need branch-level retries/backoff policies for repeated `NO_DATA`?
- Should `SharedStore` host additional synchronization primitives for concurrent API calls?
