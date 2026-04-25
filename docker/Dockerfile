# Dockerfile for reproducing the Linux CI environment locally.
# Based on ubuntu:24.04 to match the GitHub Actions ubuntu-24.04 runner.
#
# Usage:
#   docker build -t campello_widgets:linux -f docker/Dockerfile .
#   docker run --rm -v $(pwd):/workspace -w /workspace campello_widgets:linux \
#     bash -c "cmake -S . -B build/linux-docker -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build build/linux-docker && ctest --test-dir build/linux-docker --output-on-failure"

FROM ubuntu:24.04

# Prevent interactive prompts during apt-get
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and the exact dependency versions used in CI.
# GitHub Actions ubuntu-24.04 runners currently ship with GCC 13.3.0
# and CMake 3.31.6.
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    ca-certificates \
    gnupg \
    software-properties-common \
    wget \
    # Project-specific dependencies (must match .github/workflows/ci.yml)
    libvulkan-dev \
    libx11-dev \
    libdbus-1-dev \
    libfreetype6-dev \
    libharfbuzz-dev \
    libfontconfig1-dev \
    libwayland-dev \
    libxkbcommon-dev \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Upgrade CMake to 3.31.x to match the CI runner version.
# Ubuntu 24.04 ships with 3.28; GitHub Actions runners have a newer one.
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null && \
    apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Default command: print versions then drop to bash
CMD cmake --version && gcc --version && bash
