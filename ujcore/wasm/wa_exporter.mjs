// Execute this as:
// $ bazel run //ujcore/wasm:wa_exporter -- ./relative/path/to/dest

import fs from 'node:fs/promises';
import { createReadStream } from 'node:fs';
import path from "node:path";
import process from "node:process";

/**
 * Constants and path configurations extracted from Bazel environment variables.
 */
const PATH_PARAMS = {
    runFiles: process.env.JS_BINARY__RUNFILES || "",
    workspace: process.env.JS_BINARY__WORKSPACE || "",
    buildWorkDir: process.env.BUILD_WORKING_DIRECTORY || "",
    // The wasm binary rule wraps the cc binary. The the leaf files are accesible as:
    // <workspace>/<wasm binary>/<cc binary>.wasm
    codeDir: "ujcore/wasm",
    waBinary: "entrypoint_wa",
    ccBinary: "entrypoint",
};

const ALLOWED_FILE_EXTS = [".wasm", ".js"];

/**
 * Verifies if a path exists and is a regular file.
 * @param {string} filePath - Absolute path to verify.
 * @returns {Promise<boolean>} Resolves to true if file exists.
 * @throws {Error} If path is not a file or does not exist.
 */
async function verifyLeafFileExists(filePath) {
    try {
        const stats = await fs.stat(filePath);
        if (!stats.isFile()) {
            throw new Error(`Path exists but is not a file: ${filePath}`);
        }
        return true;
    } catch (err) {
        if (err.code === 'ENOENT') {
            throw new Error(`File not found: ${filePath}`);
        }
        throw err;
    }
}

/**
 * Verifies that a path exists and is a directory.
 * @param {string} dirPath - Absolute path to verify.
 * @throws {Error} If path is not a directory or does not exist.
 */
async function verifyDirectoryExists(dirPath) {
    try {
        const stats = await fs.stat(dirPath);
        if (!stats.isDirectory()) {
            throw new Error(`Path exists but is not a directory: ${dirPath}`);
        }
    } catch (err) {
        if (err.code === 'ENOENT') {
            throw new Error(`Failed to find directory: ${dirPath}`);
        }
        throw err;
    }
}

/**
 * Constructs absolute paths for source files and validates their existence and extensions.
 * @param {Object} pathParams - The PATH_PARAMS configuration object.
 * @returns {Object} { wasmBin: string, glueJs: string }
 */
function verifyAndGetFilesInfo(pathParams) {
    const { runFiles, workspace, buildWorkDir, ccBinary, waBinary, codeDir } = pathParams;

    if (runFiles.length <= 0 || workspace.length <= 0 || buildWorkDir.length <= 0) {
        console.error("Empty env params: ", { runFiles, workspace, buildWorkDir });
        process.exit(1);
    }

    const leafDir = path.join(runFiles, workspace, codeDir, waBinary);
    const wasmBinaryPath = path.join(leafDir, `${ccBinary}.wasm`);
    const glueJsPath = path.join(leafDir, `${ccBinary}.js`);

    for (const requiredFile of [wasmBinaryPath, glueJsPath]) {
        const baseName = path.basename(requiredFile);
        if (ALLOWED_FILE_EXTS.some(ext => baseName.endsWith(ext))) {
            verifyLeafFileExists(requiredFile);
        } else {
            throw new Error(`Source file ${requiredFile} has invalid extension.`);
        }
    }

    return {
        wasmBin: wasmBinaryPath,
        glueJs: glueJsPath,
    };
}

/**
 * Validates the WASM module by attempting to load it via the glue JS.
 * @param {Object} srcFilesInfo - { wasmBin: string, glueJs: string }
 */
async function loadGraphWasmModule(srcFilesInfo) {
    const { wasmBin, glueJs } = srcFilesInfo;
    const binaryStream = createReadStream(wasmBin);
    const { default: WasmModule } = await import(glueJs);
    const module = await WasmModule({ binaryStream });
    if (!module) {
        throw new Error("Failed to load WASM module");
    }
}

/**
 * Resolves the destination directory from CLI args, creates it, and cleans existing restricted files.
 * @param {string} buildWorkDir - The original CWD from Bazel.
 * @returns {Promise<string>} The resolved absolute destination path.
 */
async function prepareDestinationDir(buildWorkDir) {
    const flagDestDir = process.argv[2];
    if (!flagDestDir) {
        throw new Error("Missing destination directory argument. Usage: bazel run <target> -- <dest_dir>");
    }

    const destDir = path.isAbsolute(flagDestDir)
        ? flagDestDir
        : path.resolve(buildWorkDir, flagDestDir);

    console.log(`[EXPORT] Target destination: ${destDir}`);

    // Ensure directory exists
    await fs.mkdir(destDir, { recursive: true });

    // Clean up existing files with restricted extensions
    const existingFiles = await fs.readdir(destDir);
    for (const file of existingFiles) {
        if (ALLOWED_FILE_EXTS.some(ext => file.endsWith(ext))) {
            const fileToDelete = path.join(destDir, file);
            console.log(`[EXPORT] Cleaning up existing file: ${file}`);
            await fs.rm(fileToDelete, { force: true });
        }
    }

    await verifyDirectoryExists(destDir);

    return destDir;
}

/**
 * Copies validated source files to the destination directory.
 * @param {Object} srcFilesInfo - { wasmBin: string, glueJs: string }
 * @param {string} destDir - The target directory path.
 */
async function exportWasmFiles(srcFilesInfo, destDir) {
    const { wasmBin, glueJs } = srcFilesInfo;
    for (const srcPath of [wasmBin, glueJs]) {
        const fileName = path.basename(srcPath);
        const finalDest = path.join(destDir, fileName);
        await fs.copyFile(srcPath, finalDest);
        console.log(`[EXPORT] Copied to destination : ${fileName}`);
    }
}

/**
 * Main execution block.
 */
(async () => {
    console.log("[DEBUG] Path Params:", PATH_PARAMS);
    const srcFilesInfo = verifyAndGetFilesInfo(PATH_PARAMS);
    
    // Verify source files are valid before proceeding
    await loadGraphWasmModule(srcFilesInfo);
    
    const destDir = await prepareDestinationDir(PATH_PARAMS.buildWorkDir);
    await exportWasmFiles(srcFilesInfo, destDir);
    
    console.log("[EXPORT] Completed successfully.");
})();
