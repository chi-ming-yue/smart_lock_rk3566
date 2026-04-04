#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

DET_MODEL="${DET_MODEL:-${ROOT_DIR}/res/model/det.rknn}"
REC_MODE="${REC_MODE:-fp32}"
REC_MODEL="${REC_MODEL:-${ROOT_DIR}/res/model/rec.rknn}"
REC_MODEL_FP32="${REC_MODEL_FP32:-${ROOT_DIR}/res/model/rec_fp32.rknn}"
REC_MODEL_I8="${REC_MODEL_I8:-${ROOT_DIR}/res/model/rec_i8.rknn}"
DB_PATH="${DB_PATH:-${ROOT_DIR}/res/db/face.db}"
ROTATE="${ROTATE:-0}"
THRESHOLD="${THRESHOLD:-0.50}"
THRESHOLD_FP32="${THRESHOLD_FP32:-${THRESHOLD}}"
THRESHOLD_I8="${THRESHOLD_I8:-${THRESHOLD}}"

BIN="${ROOT_DIR}/build/bin/t_face"
IMG0="${IMG0:-${ROOT_DIR}/res/face/zw}"
IMG1="${IMG1:-${ROOT_DIR}/res/face/hjj}"

REC_MODE="${REC_MODE}" "${ROOT_DIR}/scripts/init_res.sh" >/dev/null

if [[ ! -x "${BIN}" ]]; then
    echo "missing binary: ${BIN}"
    exit 1
fi

echo "[face_smoke] image dir: ${IMG0}"
"${BIN}" \
  --det-model "${DET_MODEL}" \
  --rec-model "${REC_MODEL}" \
  --rec-model-fp32 "${REC_MODEL_FP32}" \
  --rec-model-i8 "${REC_MODEL_I8}" \
  --rec-mode "${REC_MODE}" \
  --db "${DB_PATH}" \
  --image-dir "${IMG0}" \
  --threshold-fp32 "${THRESHOLD_FP32}" \
  --threshold-i8 "${THRESHOLD_I8}" \
  --rotate "${ROTATE}"

echo "[face_smoke] image dir: ${IMG1}"
"${BIN}" \
  --det-model "${DET_MODEL}" \
  --rec-model "${REC_MODEL}" \
  --rec-model-fp32 "${REC_MODEL_FP32}" \
  --rec-model-i8 "${REC_MODEL_I8}" \
  --rec-mode "${REC_MODE}" \
  --db "${DB_PATH}" \
  --image-dir "${IMG1}" \
  --threshold-fp32 "${THRESHOLD_FP32}" \
  --threshold-i8 "${THRESHOLD_I8}" \
  --rotate "${ROTATE}"
