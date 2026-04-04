#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/out/board}"
SQL_PATH="${SQL_PATH:-${ROOT_DIR}/res/sql/face.sql}"
DB_PATH="${DB_PATH:-${ROOT_DIR}/res/db/face.db}"
DET_SRC="${DET_SRC:-${ROOT_DIR}/res/model/det.rknn}"
REC_MODE="${REC_MODE:-fp32}"
REC_FP32_SRC="${REC_FP32_SRC:-${ROOT_DIR}/res/model/rec_fp32.rknn}"
REC_I8_SRC="${REC_I8_SRC:-${ROOT_DIR}/res/model/rec_i8.rknn}"
DET_OUT="${OUT_DIR}/model/det.rknn"
REC_FP32_OUT="${OUT_DIR}/model/rec_fp32.rknn"
REC_I8_OUT="${OUT_DIR}/model/rec_i8.rknn"
REC_OUT="${OUT_DIR}/model/rec.rknn"
DB_OUT="${OUT_DIR}/db/face.db"
DBG_OUT="${OUT_DIR}/debug"

need_file() {
    local path="$1"
    if [[ ! -f "${path}" ]]; then
        echo "missing file: ${path}" >&2
        exit 1
    fi
}

mkdir -p "${ROOT_DIR}/res/db" "${OUT_DIR}/model" "${OUT_DIR}/db" "${DBG_OUT}"

need_file "${SQL_PATH}"
need_file "${DET_SRC}"
need_file "${REC_FP32_SRC}"
need_file "${REC_I8_SRC}"

if [[ ! -f "${DB_PATH}" ]]; then
    python3 "${ROOT_DIR}/scripts/mk_db.py" --sql "${SQL_PATH}" --out "${DB_PATH}"
fi
need_file "${DB_PATH}"

cp "${DET_SRC}" "${DET_OUT}"
cp "${REC_FP32_SRC}" "${REC_FP32_OUT}"
cp "${REC_I8_SRC}" "${REC_I8_OUT}"
case "${REC_MODE}" in
    i8)
        cp "${REC_I8_SRC}" "${REC_OUT}"
        ;;
    *)
        cp "${REC_FP32_SRC}" "${REC_OUT}"
        ;;
esac
cp "${DB_PATH}" "${DB_OUT}"

echo "resources ready"
echo "  det=${DET_OUT}"
echo "  rec_mode=${REC_MODE}"
echo "  rec_fp32=${REC_FP32_OUT}"
echo "  rec_i8=${REC_I8_OUT}"
echo "  rec=${REC_OUT}"
echo "  db=${DB_OUT}"
echo "  debug=${DBG_OUT}"
