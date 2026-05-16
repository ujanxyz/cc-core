/**
 * Interface for a temporary in-worker task pool for WebGPU pipelines.
 * Entries are keyed by work id. Work items are inserted by C++ execution nodes,
 * and fulfilled by the coordinating JS code that performs the GPU work in native JS
 * and resolves the pending status.
 */
export interface WebGpuTaskPoolInterface {
    /**
     * Register a pending WebGPU task and return its generated work id.
     * @param taskData Opaque task description passed from C++ or JS bridge code.
     * @returns Unique work id for later fulfillment.
     */
    registerTask(taskData: object): string;

    /**
     * Fulfill a previously registered task with result data.
     * @param workId Task id returned from registerTask.
     * @param resultData Opaque result payload produced by JS-side GPU execution.
     */
    fulfillTask(workId: string, resultData: object): void;

    /**
     * Get the original task data for a registered task.
     * @param workId Task id returned from registerTask.
     * @returns Original task payload, or null if not found.
     */
    getTaskData(workId: string): object | null;

    /**
     * Get the result data for a fulfilled task.
     * @param workId Task id returned from registerTask.
     * @returns Fulfillment payload, or null if missing/unfulfilled.
     */
    getResultData(workId: string): object | null;

    /**
     * Return the ids of tasks that are still pending fulfillment.
     * @returns Pending work ids.
     */
    getPendingTaskIds(): string[];

    /**
     * Remove a task from the pool and return its full entry.
     * Useful once C++ has consumed the completed result.
     * @param workId Task id returned from registerTask.
     * @returns Removed task entry, or null if not found.
     */
    takeTask(workId: string): PendingWebGpuTaskEntry | null;

    /**
     * Clear all registered tasks, e.g. on graph reset or worker shutdown.
     */
    clearTasks(): void;
}

/**
 * Entry representing a pending or fulfilled WebGPU task.
 */
export interface PendingWebGpuTaskEntry {
    workId: string;
    taskData: object;
    resultData: object | null;
    fulfilled: boolean;
    createdAtMs: number;
    fulfilledAtMs: number | null;
}
