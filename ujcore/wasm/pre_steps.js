const isWorker = typeof WorkerGlobalScope !== 'undefined' && globalThis instanceof WorkerGlobalScope;
if (!isWorker) {
    throw new Error("The wasm module is not running in a worker context.");
}

// This event target is used from inline JS code inside C++ / wasm.
globalThis.pipelineEvents = new EventTarget();

console.log("From pre_steps.js: ", Module);


Module.bitmapStaging = (function() {
    class BitmapStaging {
        constructor() {
        }

        addBitmap(nodeId, bitmapInfo, data) {
            console.log("BitmapStaging :: addBitmap called");
        }

    };
    console.log("BitmapStaging called");
    return new BitmapStaging();
})();