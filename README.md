# cc-core
C++ core logic for building and executing function graph

## Build and export wasm

```
bazel run //ujcore/wasm:wa_exporter -- ../svelte-app/public/webassembly
```