#!/bin/bash
#
# Install Btrfsd build dependencies
#
set -e
set -x

export DEBIAN_FRONTEND=noninteractive

# update caches
apt-get update -qq

# install build essentials
apt-get install -yq \
    eatmydata \
    build-essential \
    gdb \
    gcc \
    g++

# install build dependencies
eatmydata apt-get install -yq --no-install-recommends \
    meson \
    ninja-build \
    git \
    gettext \
    itstool \
    systemd \
    libglib2.0-dev \
    libsystemd-dev \
    gtk-doc-tools \
    libmount-dev \
    libjson-glib-dev

if apt-cache show systemd-dev > /dev/null 2>&1; then
    eatmydata apt-get install -yq systemd-dev
fi
