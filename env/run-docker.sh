#!/bin/bash

# SPDX-License-Identifier: AGPL-3.0-or-later

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Allow forcing a runtime platform (optional)
PLATFORM="${PLATFORM:-}"
if [ -n "$PLATFORM" ]; then
    PLATFORM_ARG=(--platform "$PLATFORM")
else
    PLATFORM_ARG=()
fi

# Run the ValkyrieOS Docker container with volume mounts
# Mount project root and os_toolchain directory
docker run "${PLATFORM_ARG[@]}" -it \
    -v "$PROJECT_ROOT:/valkyrie_os" \
    -v "$PROJECT_ROOT/../os_toolchain:/os_toolchain" \
    valkyrie_os