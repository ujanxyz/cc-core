#!/bin/bash
echo "BUILD_TIMESTAMP $(date +%s)"
echo "BUILD_HOST $(hostname)"
echo "BUILD_PLATFORM $(uname)"
echo "BUILD_USER $(whoami)"
