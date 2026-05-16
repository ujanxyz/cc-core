const isWorker = typeof WorkerGlobalScope !== 'undefined' && globalThis instanceof WorkerGlobalScope;
if (!isWorker) {
    throw new Error("The wasm module is not running in a worker context.");
}

// This event target is used from inline JS code inside C++ / wasm.
globalThis.pipelineEvents = new EventTarget();

console.log("From pre_steps.js: ", Module);

Module.assetStaging = (function() {
    /**
     * @typedef {object} StagedImageEntry
     * @property {string} assetUri
     * @property {ImageData} bitmap
     */

    /**
     * Temporary in-worker staging for mutable bitmaps that are prepared for WASM input.
     * Entries are keyed by slot id and store both the source asset URI and ImageData.
     */
    class AssetStaging {
        constructor() {
            /** @type {Map<string, StagedImageEntry>} */
            this._entriesBySlot = new Map();
        }

        /**
         * Stage a bitmap (ImageData) for a specific slot.
         *
         * @param {string} slotIdStr A string id specifying the expected consuming slot.
         * @param {string} assetUri The URI of the backing asset.
         * @param {ImageData} bitmap The ImageData to be staged.
         * @returns {void}
         */
        stageImageData(slotIdStr, assetUri, bitmap) {
            if (typeof slotIdStr !== "string" || slotIdStr.trim() === "") {
                throw new Error("stageImageData: slotIdStr must be a non-empty string");
            }
            if (typeof assetUri !== "string" || assetUri.trim() === "") {
                throw new Error("stageImageData: assetUri must be a non-empty string");
            }
            if (!(bitmap instanceof ImageData)) {
                throw new Error("stageImageData: bitmap must be an ImageData instance");
            }

            this._entriesBySlot.set(slotIdStr, { assetUri, bitmap });
        }

        /**
         * Release the bitmap staged for the slot and verify it came from the expected asset URI.
         *
         * @param {string} slotIdStr A string id specifying the expected consuming slot.
         * @param {string} assetUri The URI of the backing asset expected for this slot.
         * @returns {ImageData | null} Released image when found+verified, else null when no slot is staged.
         */
        releaseImageData(slotIdStr, assetUri) {
            const entry = this._entriesBySlot.get(slotIdStr);
            if (!entry) {
                return null;
            }

            if (entry.assetUri !== assetUri) {
                throw new Error(
                    `releaseImageData: asset URI mismatch for slot '${slotIdStr}'. ` +
                    `Expected '${entry.assetUri}', got '${assetUri}'.`,
                );
            }

            this._entriesBySlot.delete(slotIdStr);
            console.log("[Pre-js]: Released staged bitmap for slot:", slotIdStr, " with asset URI: ", assetUri);
            return entry.bitmap;
        }

        /**
         * Clear all staged assets, e.g. on graph reset or worker shutdown.
         *
         * @returns {void}
         */
        clearAssets() {
            this._entriesBySlot.clear();
        }
    };

    return new AssetStaging();
})();



Module.wgpuTaskPool = (function() {
    /**
     * @typedef {object} PendingWebGpuTaskEntry
     * @property {string} workId
     * @property {object} taskData
     * @property {object | null} resultData
     * @property {boolean} fulfilled
     * @property {number} createdAtMs
     * @property {number | null} fulfilledAtMs
     */

    /**
     * Temporary in-worker taskpool for WebGPU pipelines. Entries are keyed by work id.
     * Work items are inserted by C++ execution nodes, and fulfilled by the coordinating
     * JS code that performs the GPU work in native JS and resolves the pending status.
     */
    class WebGpuTaskPool {
        constructor() {
            /** @type {Map<string, PendingWebGpuTaskEntry>} */
            this._entriesByWorkId = new Map();
        }

        /**
         * Register a pending WebGPU task and return its generated work id.
         *
         * @param {object} taskData Opaque task description passed from C++ or JS bridge code.
         * @returns {string} Unique work id for later fulfillment.
         */
        registerTask(taskData) {
            if (taskData === null || typeof taskData !== "object" || Array.isArray(taskData)) {
                throw new Error("registerTask: taskData must be a non-null object");
            }

            const workId = (globalThis.crypto && globalThis.crypto.randomUUID)
                ? globalThis.crypto.randomUUID()
                : `wgpu-${Date.now()}-${Math.floor(Math.random() * 1000000)}`;

            /** @type {PendingWebGpuTaskEntry} */
            const entry = {
                workId,
                taskData,
                resultData: null,
                fulfilled: false,
                createdAtMs: Date.now(),
                fulfilledAtMs: null,
            };

            this._entriesByWorkId.set(workId, entry);
            return workId;
        }

        /**
         * Fulfill a previously registered task with result data.
         *
         * @param {string} workId Task id returned from registerTask.
         * @param {object} resultData Opaque result payload produced by JS-side GPU execution.
         * @returns {void}
         */
        fulfillTask(workId, resultData) {
            if (typeof workId !== "string" || workId.trim() === "") {
                throw new Error("fulfillTask: workId must be a non-empty string");
            }
            if (resultData === null || typeof resultData !== "object" || Array.isArray(resultData)) {
                throw new Error("fulfillTask: resultData must be a non-null object");
            }

            const entry = this._entriesByWorkId.get(workId);
            if (!entry) {
                throw new Error(`fulfillTask: unknown workId '${workId}'`);
            }
            if (entry.fulfilled) {
                throw new Error(`fulfillTask: workId '${workId}' was already fulfilled`);
            }

            entry.resultData = resultData;
            entry.fulfilled = true;
            entry.fulfilledAtMs = Date.now();
        }

        /**
         * Get the original task data for a registered task.
         *
         * @param {string} workId Task id returned from registerTask.
         * @returns {object | null} Original task payload, or null if not found.
         */
        getTaskData(workId) {
            if (typeof workId !== "string" || workId.trim() === "") {
                throw new Error("getTaskData: workId must be a non-empty string");
            }

            const entry = this._entriesByWorkId.get(workId);
            return entry ? entry.taskData : null;
        }

        /**
         * Get the result data for a fulfilled task.
         *
         * @param {string} workId Task id returned from registerTask.
         * @returns {object | null} Fulfillment payload, or null if missing/unfulfilled.
         */
        getResultData(workId) {
            if (typeof workId !== "string" || workId.trim() === "") {
                throw new Error("getResultData: workId must be a non-empty string");
            }

            const entry = this._entriesByWorkId.get(workId);
            if (!entry || !entry.fulfilled) {
                return null;
            }
            return entry.resultData;
        }

        /**
         * Return the ids of tasks that are still pending fulfillment.
         *
         * @returns {string[]} Pending work ids.
         */
        getPendingTaskIds() {
            const pendingIds = [];
            for (const [workId, entry] of this._entriesByWorkId.entries()) {
                if (!entry.fulfilled) {
                    pendingIds.push(workId);
                }
            }
            return pendingIds;
        }

        /**
         * Remove a task from the pool and return its full entry.
         * Useful once C++ has consumed the completed result.
         *
         * @param {string} workId Task id returned from registerTask.
         * @returns {PendingWebGpuTaskEntry | null} Removed task entry, or null if not found.
         */
        takeTask(workId) {
            if (typeof workId !== "string" || workId.trim() === "") {
                throw new Error("takeTask: workId must be a non-empty string");
            }

            const entry = this._entriesByWorkId.get(workId);
            if (!entry) {
                return null;
            }
            this._entriesByWorkId.delete(workId);
            return entry;
        }

        /**
         * Clear all registered tasks, e.g. on graph reset or worker shutdown.
         *
         * @returns {void}
         */
        clearTasks() {
            this._entriesByWorkId.clear();
        }
    };

    return new WebGpuTaskPool();
})();