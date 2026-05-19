const isWorker = typeof WorkerGlobalScope !== 'undefined' && globalThis instanceof WorkerGlobalScope;
if (!isWorker) {
    throw new Error("The wasm module is not running in a worker context.");
}

// This event target is used from inline JS code inside C++ / wasm.
globalThis.pipelineEvents = new EventTarget();

console.log("From pre_steps.js: ", Module);

//--------------------------------------------------------------------------------------------------
Module.awaitPool = (function() {
    /**
     * In-memory implementation of AwaitTaskPool for plain JavaScript runtimes.
     *
     * This class expects registered channels to expose:
     * - addTask(taskData): taskId
     * - peekResult(taskId): resultData | null  
     * - releaseResult(taskId, forceDelete?): resultData | null
     */
    class AwaitTaskPool {
        constructor() {
            /** @type {Map<string, any>} */
            this.channels = new Map();
        }

        /**
         * Adds a task to the specified channel and returns a task id.
         * @param {string} channel
         * @param {object} taskData
         * @returns {string}
         */
        addTask(channel, taskData) {
            const channelRef = this.getChannel(channel);
            return channelRef.addTask(taskData);
        }

        /**
         * Returns fulfilled task result without removing it.
         * @param {string} channel
         * @param {string} taskId
         * @returns {object | null}
         */
        peekResult(channel, taskId) {
            const channelRef = this.getChannel(channel);
            if (typeof channelRef.peekResult !== "function") {
            throw new Error(
                `Channel "${channel}" does not support result peeking. ` +
                "Provide a channel with peekResult(taskId).",
            );
            }
            return channelRef.peekResult(taskId);
        }

        /**
         * Returns fulfilled result and releases task state.
         * @param {string} channel
         * @param {string} taskId
         * @param {boolean} [forceDelete=false]
         * @returns {object | null}
         */
        releaseResult(channel, taskId, forceDelete = false) {
            const channelRef = this.getChannel(channel);
            if (typeof channelRef.releaseResult !== "function") {
            throw new Error(
                `Channel "${channel}" does not support result release. ` +
                "Provide a channel with releaseResult(taskId, forceDelete).",
            );
            }
            return channelRef.releaseResult(taskId, forceDelete);
        }

        /**
         * Registers or replaces a channel by name.
         * @param {string} channelName
         * @param {any} channel
         */
        registerChannel(channelName, channel) {
            this.channels.set(channelName, channel);
        }

        /**
         * @param {string} channelName
         * @returns {any}
         */
        getChannel(channelName) {
            const channel = this.channels.get(channelName);
            if (!channel) {
            throw new Error(`Unknown AwaitTaskChannel: ${channelName}`);
            }
            return channel;
        }
    };

    return new AwaitTaskPool();
})();


