# Implement function return type.

Implement a C++ data structur to be used as function return types for various cases.

This is a node-based execution pipeline which executes C++ in WASM (web-assembly). Some function need native JS logic and can call embedded JS via `EM_JS`. However async JC logic cannot be called this way. Therefore a function in C++ which wants to call some async JS logic will be
interrupted, the control is returned to the calling JS (Wasm glue code) which will perform the requested async logic, and the re-invoke the function telling it to resume execution from where it left.

Internally such function will do partial computation, save the state and return a special return
code, which is not an error, but tells the pipeline stages coordinator to perform the associated pure JS async code (the coordinator is always running async JS in a web-worker), and re-invoke the same function instance to let it continue and complete.

One classic example is WebGPU function. In the graph all other nodes are C++ functions, but one node is WebGPU render. We decided not to use the emscripten webgpu headers, rather run the stage in WebWorker JS. WebGPU JS code is inherently async, specially while reading back the GPU buffer. So the webgpu function node will stage the webgpu work using `EM_JS`, and yield the control with some form of id for the pending async JS work. Assume the actual WebGPU JS code will be implemented in a JS library and attached to teh WASM module.

So the possible return values from a function will be:

- **DONE** The execution is complete and the results are available in the designated slots.
- **NO_DATA** Waiting for input, some input slot is not populated (either no edge or manual data, or no data along the incoming edge)
- **AWAIT** This is the special status where the function yielded control and needs to be invoked again to complete.
- **ERROR** Error code and error message.


