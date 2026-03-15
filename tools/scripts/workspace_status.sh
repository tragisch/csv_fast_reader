#!/bin/bash
# Script to generate volatile status files for Bazel builds
# This script provides build metadata like commit hash, branch, etc.

# Git commit hash
if git rev-parse --git-dir > /dev/null 2>&1; then
    COMMIT_HASH=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
    BRANCH_NAME=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
    COMMIT_DATE=$(git log -1 --format=%cd --date=short 2>/dev/null || echo "unknown")
else
    COMMIT_HASH="unknown"
    BRANCH_NAME="unknown"
    COMMIT_DATE="unknown"
fi

# Print status as key-value pairs
echo "COMMIT_HASH ${COMMIT_HASH}"
echo "BRANCH_NAME ${BRANCH_NAME}"
echo "COMMIT_DATE ${COMMIT_DATE}"
echo "BUILD_TIMESTAMP $(date -u +%Y-%m-%dT%H:%M:%SZ)"
