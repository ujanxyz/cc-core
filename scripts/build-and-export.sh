#!/bin/bash

APP_REPO="../svelte-app"

bazel run //ujcore/wasm:wa_exporter -- "${APP_REPO}/public/webassembly"
