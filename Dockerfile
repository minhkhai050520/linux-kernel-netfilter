FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install all dependencies for Linux kernel build & ARM64 cross-compilation
RUN apt-get update && apt-get install -y \
    # 1. Toolchain & basic build tools
    build-essential \
    gcc \
    g++ \
    make \
    bison \
    flex \
    # 2. Cross-compilers for ARM64 (aarch64) and ARM32 (armhf)
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    # 3. Header libraries & tools for generating BTF/debug info for kernel (required)
    libncurses-dev \
    libssl-dev \
    libelf-dev \
    dwarves \
    pahole \
    kmod \
    # 4. Packaging, compression & build system utilities
    bash \
    bc \
    binutils \
    bzip2 \
    cpio \
    diffutils \
    file \
    findutils \
    git \
    gzip \
    patch \
    perl \
    python3 \
    python3-pip \
    rsync \
    sed \
    tar \
    unzip \
    wget \
    xz-utils \
    locales \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Configure UTF-8 locale
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

# Sync UID/GID with host user to avoid permission errors when writing files to workspace
ARG USER_ID=1000
ARG GROUP_ID=1000

RUN groupadd -g ${GROUP_ID} kernel_builder && \
    useradd -m -u ${USER_ID} -g ${GROUP_ID} -s /bin/bash kernel_builder && \
    echo "kernel_builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER kernel_builder
WORKDIR /workspace