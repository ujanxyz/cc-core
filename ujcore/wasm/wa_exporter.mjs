// Execute this as:
// $ bazel run //ujcore/wasm:wa_exporter

import fs from 'node:fs/promises';
import { createReadStream } from 'node:fs';
import path from "path";
import process from "process";

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

const ALLOWED_FILE_EXTS = [".wasm", ".js"];  // '.glue.js'

/**
 * Verifies if a path exists and is a regular file.
 * @param {string} filePath - Absolute path to verify
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
        throw err; // Re-throw other errors like EACCES (Permissions)
    }
}

function verifyAndGetFilesInfo(pathParams) {
  const {runFiles, workspace, buildWorkDir} = pathParams;
  if (runFiles.length <= 0 || workspace.length <= 0 || buildWorkDir.length <= 0) {
    console.error("Empty env params: ", {runFiles, workspace, buildWorkDir});
    process.exit(1);
  }
  const {ccBinary} = pathParams;
  const leafDir = path.join(pathParams.runFiles, pathParams.workspace, pathParams.codeDir, pathParams.waBinary);
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
  }
}

//------------------------------------------------------------------------------
/**
 * @param {Object} srcFilesInfo - { wasmBin: string, glueJs: string }
 */
async function loadGraphWasmModule(srcFilesInfo) {
  const {wasmBin, glueJs} = srcFilesInfo;
  const binaryStream = createReadStream(wasmBin);
  const { default: WasmModule } = await import(glueJs);
  const module = await WasmModule({ binaryStream });
  if (!module) {
    throw new Error("Failed to load WASM module");
  }
  // The module has been verified, delete it.
  // delete module;
}
//------------------------------------------------------------------------------
async function prepareDestinationDir(buildWorkDir) {
    // Get the CLI flag (assuming it's the first argument after node and script)
    const flagDestDir = process.argv[2];
    if (!flagDestDir) {
        throw new Error("Missing destination directory argument.");
    }
    const destDir = path.isAbsolute(flagDestDir)
        ? flagDestDir 
        : path.resolve(buildWorkDir, flagDestDir);
    console.log(`[EXPORT] Target destination: ${destDir}`);
    await fs.mkdir(destDir, { recursive: true });
    const existingFiles = await fs.readdir(destDir);
    for (const file of existingFiles) {
        if (ALLOWED_FILE_EXTS.some(ext => file.endsWith(ext))) {
            const fileToDelete = path.join(destDir, file);
            console.log(`[EXPORT] Cleaning up existing file: ${file}`);
            // Use force: true to avoid throwing if the file disappears mid-process
            await fs.rm(fileToDelete, { force: true });
        }
    }

    // TODO: Refactor into a method verifyDirectoryExists
    try {
        const stats = await fs.stat(destDir);
        if (!stats.isDirectory()) {
            throw new Error(`Path exists but is not a directory: ${destDir}`);
        }
    } catch (err) {
        if (err.code === 'ENOENT') {
            throw new Error(`Failed to create or find directory: ${destDir}`);
        }
        throw err;
    }

    return destDir;
}

//------------------------------------------------------------------------------
/**
 * @param {Object} srcFilesInfo - { wasmBin: string, glueJs: string }
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

//------------------------------------------------------------------------------
(async () => {
  console.log(PATH_PARAMS);
  const srcFilesInfo = verifyAndGetFilesInfo(PATH_PARAMS);
  await loadGraphWasmModule(srcFilesInfo);
  const destDir = await prepareDestinationDir(PATH_PARAMS.buildWorkDir);
  exportWasmFiles(srcFilesInfo, destDir);
})();
