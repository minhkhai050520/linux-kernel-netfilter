#!/usr/bin/env bash

set -euo pipefail

IMAGE_NAME="linux-kernel-build-env"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

USER_ID=$(id -u)
GROUP_ID=$(id -g)

echo "========================================================"
echo "[HOST] Building Docker image: $IMAGE_NAME (UID: $USER_ID, GID: $GROUP_ID)"
echo "========================================================"
docker build \
    --build-arg USER_ID="$USER_ID" \
    --build-arg GROUP_ID="$GROUP_ID" \
    -t "$IMAGE_NAME" "$PROJECT_DIR"

echo "========================================================"     
echo "[HOST] Starting Docker container with bind mount to $PROJECT_DIR"
echo "========================================================"
docker run --rm -it \
    -v "$PROJECT_DIR":/workspace \
    -w /workspace \
    "$IMAGE_NAME"
