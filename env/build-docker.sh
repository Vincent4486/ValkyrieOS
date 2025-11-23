#!/bin/bash

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Allow forcing a build platform (useful on Apple Silicon)
PLATFORM="${PLATFORM:-linux/amd64}"
echo "Building Docker image for platform: $PLATFORM"

# Build the ValkyrieOS Docker image from project root
docker build --platform "$PLATFORM" -t valkyrie_os -f "$SCRIPT_DIR/Dockerfile" "$PROJECT_ROOT"
