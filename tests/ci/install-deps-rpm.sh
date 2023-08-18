#!/bin/sh
#
# Install Btrfsd build dependencies
#
set -e
set -x

# update caches
dnf makecache

# install build dependencies
dnf --assumeyes --quiet --setopt=install_weak_deps=False install \
    gcc \
    gcc-c++ \
    gdb \
    git \
    meson \
    gettext \
    gtk-doc \
    libasan \
    libubsan \
    systemd \
    'pkgconfig(gio-2.0)' \
    'pkgconfig(libsystemd)' \
    'pkgconfig(mount)' \
    'pkgconfig(json-glib-1.0)' \
