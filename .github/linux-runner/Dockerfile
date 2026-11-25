# Self-hosted GitHub Actions runner that builds Chimera on Linux.
# Holds clang, CMake, Ninja, ccache, the Vulkan SDK (dxc for the shaders) and the
# development packages SDL3 needs to build with X11, Wayland and audio.

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG CLANG_VERSION=18
# "latest" asks LunarG and GitHub for the newest version when the image is built.
ARG VULKAN_SDK_VERSION=latest
ARG RUNNER_VERSION=latest

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl git jq xz-utils \
      build-essential cmake ninja-build ccache python3 pkg-config \
      clang-${CLANG_VERSION} lld-${CLANG_VERSION} \
      libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
      libxss-dev libxtst-dev libxkbcommon-dev \
      libwayland-dev wayland-protocols libdecor-0-dev \
      libegl-dev libgl-dev libgles-dev libdrm-dev libgbm-dev \
      libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev \
 && ln -sf /usr/bin/clang-${CLANG_VERSION} /usr/local/bin/clang \
 && ln -sf /usr/bin/clang++-${CLANG_VERSION} /usr/local/bin/clang++ \
 && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    version="${VULKAN_SDK_VERSION}"; \
    if [ "$version" = "latest" ]; then \
      version="$(curl -fsSL https://vulkan.lunarg.com/sdk/latest/linux.txt)"; \
    fi; \
    mkdir -p /opt/vulkan; cd /opt/vulkan; \
    curl -fsSL -o sdk.tar.xz \
      "https://sdk.lunarg.com/sdk/download/${version}/linux/vulkansdk-linux-x86_64-${version}.tar.xz"; \
    tar -xf sdk.tar.xz; rm sdk.tar.xz; \
    ln -s "/opt/vulkan/${version}/x86_64" /opt/vulkan/current

ENV VULKAN_SDK=/opt/vulkan/current \
    PATH=/opt/vulkan/current/bin:$PATH \
    LD_LIBRARY_PATH=/opt/vulkan/current/lib

RUN useradd --create-home --uid 1001 runner

WORKDIR /home/runner/actions-runner
RUN set -eux; \
    version="${RUNNER_VERSION}"; \
    if [ "$version" = "latest" ]; then \
      version="$(curl -fsSL https://api.github.com/repos/actions/runner/releases/latest \
                 | jq -r .tag_name)"; \
      version="${version#v}"; \
    fi; \
    curl -fsSL -o runner.tar.gz \
      "https://github.com/actions/runner/releases/download/v${version}/actions-runner-linux-x64-${version}.tar.gz"; \
    tar -xzf runner.tar.gz; rm runner.tar.gz; \
    ./bin/installdependencies.sh; \
    rm -rf /var/lib/apt/lists/*

# The volumes take these folders' owner the first time they are mounted.
RUN mkdir -p /home/runner/state /home/runner/work /home/runner/.ccache \
 && chown -R runner:runner /home/runner

COPY --chown=runner:runner entrypoint.sh /home/runner/entrypoint.sh
RUN chmod +x /home/runner/entrypoint.sh

ENV CCACHE_DIR=/home/runner/.ccache \
    CCACHE_MAXSIZE=20G \
    CMAKE_C_COMPILER_LAUNCHER=ccache \
    CMAKE_CXX_COMPILER_LAUNCHER=ccache

USER runner
ENTRYPOINT ["/home/runner/entrypoint.sh"]
