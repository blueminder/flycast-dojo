FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && apt-get -y install \
        build-essential \
        ccache \
        cmake \
        git \
        ninja-build \
        pkg-config \
        wget \
        patchelf \
        # core emulator deps (from CI)
        libao-dev \
        libasound2-dev \
        libevdev-dev \
        libgl1-mesa-dev \
        liblua5.3-dev \
        libminiupnpc-dev \
        libpulse-dev \
        libsdl2-dev \
        libudev-dev \
        libzip-dev \
        libcurl4-openssl-dev \
        libasio-dev \
        # bundled SDL build deps (from CI)
        libwayland-dev \
        libaudio-dev \
        libjack-dev \
        libsndio-dev \
        libsamplerate0-dev \
        libx11-dev \
        libxext-dev \
        libxrandr-dev \
        libxcursor-dev \
        libxfixes-dev \
        libxi-dev \
        libxss-dev \
        libxkbcommon-dev \
        libdrm-dev \
        libgbm-dev \
        libgles2-mesa-dev \
        libegl1-mesa-dev \
        libdbus-1-dev \
        libibus-1.0-dev \
        fcitx-libs-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
