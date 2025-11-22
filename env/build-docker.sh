#!/bin/bash

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Build the ValkyrieOS Docker image from project root
docker build -t valkyrie_os -f "$SCRIPT_DIR/Dockerfile" "$PROJECT_ROOT"
