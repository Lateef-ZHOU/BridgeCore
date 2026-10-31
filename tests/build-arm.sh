#!/bin/sh
set -eu
# Existing PetaLinux-compatible SDK container. No install, root-image write or service change.
# Mount this checkout at /src; native fixed dependency cache at /cache;
# cdl5Device at /cdl5; DmaProxy at /proxy; board SDK/sysroot at /work;
# cached CMake distribution at /tools; writable build output at /out.
export PATH=/tools/cmake-3.31.6-linux-x86_64/bin:$PATH
export PKG_CONFIG_LIBDIR=/opt/lidar-runtime/lib/pkgconfig
prefix=/opt/lidar-runtime
base="--sysroot=/work/sysroot -B/work/sysroot/usr/lib/aarch64-linux-gnu/ -L/work/sysroot/usr/lib/aarch64-linux-gnu -L/work/sysroot/lib/aarch64-linux-gnu"
cxx="$base -nostdinc++ -isystem $prefix/include/c++/12.2.0 -isystem $prefix/include/c++/12.2.0/aarch64-linux-gnu -isystem $prefix/include/c++/12.2.0/backward"
link="-L$prefix/lib $base -Wl,-rpath-link,$prefix/lib"
cmake -S /src -B /out/build -GNinja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/work/toolchain.cmake -DCMAKE_CXX_FLAGS="$cxx" \
    -DCMAKE_EXE_LINKER_FLAGS="$link" -DCMAKE_SHARED_LINKER_FLAGS="$link" \
    -DCMAKE_PREFIX_PATH="$prefix" -DQT_HOST_PATH=/work/hostqt \
    -DFETCHCONTENT_SOURCE_DIR_LIDAR_HANDLE_CORE=/cache/lidar_handle_core-src \
    -DFETCHCONTENT_SOURCE_DIR_SQAF=/cache/sqaf-src \
    -DFETCHCONTENT_SOURCE_DIR_WHLS=/cache/whls-src \
    -DFETCHCONTENT_SOURCE_DIR_BOOST=/cache/boost-src \
    -DFETCHCONTENT_SOURCE_DIR_CDL5=/cdl5 -DCDL5_DMA_PROXY_SOURCE_DIR=/proxy \
    -DBUILD_TESTING=ON
cmake --build /out/build -j4
aarch64-linux-gnu-readelf -h -d -V /out/build/bridge
