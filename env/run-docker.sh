#!/bin/bash

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Run the ValkyrieOS Docker container with volume mounts
# Mount project root and os_toolchain directory
docker run -it \
    -v "$PROJECT_ROOT:/valkyrie_os" \
    -v "$PROJECT_ROOT/../os_toolchain:/os_toolchain" \
    valkyrie_os