const isWorker = typeof WorkerGlobalScope !== 'undefined' && globalThis instanceof WorkerGlobalScope;
if (!isWorker) {
    throw new Error("The wasm module is not running in a worker context.");
}

// This event target is used from inline JS code inside C++ / wasm.
globalThis.pipelineEvents = new EventTarget();
