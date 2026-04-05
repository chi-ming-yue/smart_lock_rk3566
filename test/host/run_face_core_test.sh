#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="${ROOT}/build-host/face_core_test"
mkdir -p "${ROOT}/build-host"
TOOLCHAIN_ROOT=${TOOLCHAIN_ROOT:-/opt/atk-dlrk356x-toolchain}
SYSROOT=${SYSROOT:-${TOOLCHAIN_ROOT}/aarch64-buildroot-linux-gnu/sysroot}
CXX=${CXX:-${TOOLCHAIN_ROOT}/bin/aarch64-buildroot-linux-gnu-g++}

"${CXX}" --sysroot="${SYSROOT}" -std=c++11 -Wall -Wextra -O2 \
  -I"${ROOT}/third/rknn_face/src" \
  -I"${ROOT}/face/pipe/include" \
  -I"${ROOT}/third/hnswlib" \
  -I"${SYSROOT}/usr/include" \
  "${ROOT}/test/host/face_core_test.cpp" \
  "${ROOT}/third/rknn_face/src/face_database.cpp" \
  "${ROOT}/third/rknn_face/src/face_recognizer.cpp" \
  "${ROOT}/face/pipe/src/face_vote.cpp" \
  -L"${SYSROOT}/usr/lib" \
  -lsqlite3 -lcrypto -lpthread \
  -o "${OUT}"

qemu-aarch64-static -L "${SYSROOT}" "${OUT}"
