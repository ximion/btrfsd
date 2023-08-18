#!/bin/sh
set -e

# This script is supposed to run inside the Btrfsd Docker container
# on the CI system.

#
# Read options for the current test build
#

. /etc/os-release
static_analysis=false

build_type=debugoptimized
build_dir="cibuild"
sanitize_flag=""
if [ "$1" = "sanitize" ]; then
    build_dir="cibuild-san"
    # FIXME: Build withour GIR, as g-ir-scanner hangs endlessly when using asan
    sanitize_flags="-Db_sanitize=address,undefined -Dgir=false"
    build_type=debug
    echo "Running build with sanitizers 'address,undefined' enabled."
    # Slow unwind, but we get better backtraces
    export ASAN_OPTIONS=fast_unwind_on_malloc=0

    echo "Running static analysis during build."
    static_analysis=true
fi;
if [ "$1" = "codeql" ]; then
    build_type=debug
fi;

echo "C compiler: $CC"
echo "C++ compiler: $CXX"
set -x
$CC --version

#
# Configure Btrfsd build
#

mkdir $build_dir && cd $build_dir
meson --buildtype=$build_type \
      $sanitize_flags \
      -Dmaintainer=true \
      -Dstatic-analysis=$static_analysis \
      ..

#
# Build & Install
#

ninja
DESTDIR=/tmp/install_root/ ninja install
rm -r /tmp/install_root/
