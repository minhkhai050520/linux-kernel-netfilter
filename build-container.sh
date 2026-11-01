#!/usr/bin/env bash

set -euo pipefail

IMAGE_NAME="linux-kernel-build-env"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN_DIR="${PROJECT_DIR}/toolchains/bootlin-gcc14"

# 1. Automatically check and download Bootlin GCC 14 Toolchain if not present on the host
if [ ! -f "${TOOLCHAIN_DIR}/bin/aarch64-linux-gcc" ]; then

    mkdir -p "${TOOLCHAIN_DIR}"

    # Download Bootlin GCC 14 Toolchain (stable 2025.08-1)
    wget -q --show-progress \
        https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--glibc--stable-2025.08-1.tar.xz \
        -O /tmp/gcc14.tar.xz

    tar -xJf /tmp/gcc14.tar.xz -C "${TOOLCHAIN_DIR}" --strip-components=1
    rm -f /tmp/gcc14.tar.xz
fi

USER_ID=$(id -u)
GROUP_ID=$(id -g)

# Building Docker image $IMAGE_NAME
docker build \
    --build-arg USER_ID="$USER_ID" \
    --build-arg GROUP_ID="$GROUP_ID" \
    -t "$IMAGE_NAME" "$PROJECT_DIR"

# Starting Docker container with bind mount to $PROJECT_DIR"
docker run --rm -it \
    -v "$PROJECT_DIR":/workspace \
    -w /workspace \
    "$IMAGE_NAME"
