#!/usr/bin/env bash
# Ubuntu 22.04 has no libjxl package. Build the exact upstream release instead.
set -euo pipefail
paf_jxl=${1:-/tmp/paf-jxl}
paf_cmake=${PAF_CMAKE:-cmake}
paf_commit=794a5dcf0d54f9f0b20d288a12e87afb91d20dfc
mkdir -p "$paf_jxl"
if [ ! -d "$paf_jxl/source/.git" ]; then
  git clone --depth 1 --branch v0.11.1 https://github.com/libjxl/libjxl.git "$paf_jxl/source"
fi
[ "$(git -C "$paf_jxl/source" rev-parse HEAD)" = "$paf_commit" ]
git -C "$paf_jxl/source" submodule update --init --depth 1 third_party/brotli third_party/highway third_party/lcms
"$paf_cmake" -S "$paf_jxl/source" -B "$paf_jxl/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$paf_jxl/install" \
  -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
  -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_JPEGLI=OFF \
  -DJPEGXL_ENABLE_SKCMS=OFF -DJPEGXL_ENABLE_DOXYGEN=OFF \
  -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF \
  -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF \
  -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_OPENEXR=OFF
"$paf_cmake" --build "$paf_jxl/build" --parallel 4
"$paf_cmake" --install "$paf_jxl/build"
