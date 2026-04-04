#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ADB="${ADB:-adb}"
REMOTE_DIR="${REMOTE_DIR:-/userdata/smart_lock}"
BOARD_RES="${BOARD_RES:-${ROOT_DIR}/out/board}"
DET_MODEL="${DET_MODEL:-${REMOTE_DIR}/model/det.rknn}"
REC_MODE="${REC_MODE:-fp32}"
REC_MODEL="${REC_MODEL:-${REMOTE_DIR}/model/rec.rknn}"
REC_MODEL_FP32="${REC_MODEL_FP32:-${REMOTE_DIR}/model/rec_fp32.rknn}"
REC_MODEL_I8="${REC_MODEL_I8:-${REMOTE_DIR}/model/rec_i8.rknn}"
DB_PATH="${DB_PATH:-${REMOTE_DIR}/db/face.db}"
DEBUG_DIR="${DEBUG_DIR:-${REMOTE_DIR}/debug}"
THRESHOLD_FP32="${THRESHOLD_FP32:-0.58}"
THRESHOLD_I8="${THRESHOLD_I8:-0.50}"

run_adb() {
    "${ADB}" "$@"
}

push_file() {
    local src="$1"
    local dst="$2"
    if [[ ! -f "${src}" ]]; then
        echo "missing file: ${src}"
        exit 1
    fi
    run_adb push "${src}" "${dst}"
}

deploy() {
    REC_MODE="${REC_MODE}" "${ROOT_DIR}/scripts/init_res.sh" >/dev/null
    run_adb shell "mkdir -p ${REMOTE_DIR}/model ${REMOTE_DIR}/db ${DEBUG_DIR}"

    push_file "${ROOT_DIR}/build/bin/ctl" "${REMOTE_DIR}/ctl"
    push_file "${ROOT_DIR}/build/bin/t_state" "${REMOTE_DIR}/t_state"
    push_file "${ROOT_DIR}/build/bin/t_run" "${REMOTE_DIR}/t_run"
    push_file "${ROOT_DIR}/build/bin/t_dev" "${REMOTE_DIR}/t_dev"
    push_file "${ROOT_DIR}/build/bin/t_face" "${REMOTE_DIR}/t_face"
    push_file "${ROOT_DIR}/build/bin/t_flow" "${REMOTE_DIR}/t_flow"
    push_file "${ROOT_DIR}/build/bin/t_key" "${REMOTE_DIR}/t_key"
    push_file "${ROOT_DIR}/build/bin/t_sensor" "${REMOTE_DIR}/t_sensor"
    push_file "${ROOT_DIR}/build/bin/t_oled" "${REMOTE_DIR}/t_oled"
    push_file "${ROOT_DIR}/build/bin/t_servo" "${REMOTE_DIR}/t_servo"
    push_file "${ROOT_DIR}/build/kmod/mydev.ko" "${REMOTE_DIR}/mydev.ko"
    push_file "${BOARD_RES}/model/det.rknn" "${DET_MODEL}"
    push_file "${BOARD_RES}/model/rec_fp32.rknn" "${REC_MODEL_FP32}"
    push_file "${BOARD_RES}/model/rec_i8.rknn" "${REC_MODEL_I8}"
    push_file "${BOARD_RES}/model/rec.rknn" "${REC_MODEL}"
    push_file "${BOARD_RES}/db/face.db" "${DB_PATH}"

    run_adb shell "chmod +x ${REMOTE_DIR}/ctl ${REMOTE_DIR}/t_*"
    run_adb shell "rmmod mydev >/dev/null 2>&1 || true"
    run_adb shell "insmod ${REMOTE_DIR}/mydev.ko"
}

auto_tests() {
    run_adb shell "${REMOTE_DIR}/t_dev --led on --oled demo 0.90 --servo open"
    run_adb shell "${REMOTE_DIR}/t_oled --face demo 0.90"
    run_adb shell "${REMOTE_DIR}/t_servo --loops 1"
}

sensor_test() {
    run_adb shell "${REMOTE_DIR}/t_sensor --polls 200 --poll-ms 100"
}

key_test() {
    run_adb shell "${REMOTE_DIR}/t_key --polls 200 --poll-ms 100"
}

ctl_keypad() {
    run_adb shell "${REMOTE_DIR}/ctl --keypad-only --loop"
}

ctl_face() {
    run_adb shell "${REMOTE_DIR}/ctl --loop --face-match-required --camera 0 --camera-width 1280 --camera-height 720 --camera-rotate 180 --det-model ${DET_MODEL} --rec-model ${REC_MODEL} --rec-model-fp32 ${REC_MODEL_FP32} --rec-model-i8 ${REC_MODEL_I8} --rec-mode ${REC_MODE} --threshold-fp32 ${THRESHOLD_FP32} --threshold-i8 ${THRESHOLD_I8} --db ${DB_PATH} --debug-face-dir ${DEBUG_DIR}"
}

ctl_face_resume() {
    run_adb shell "${REMOTE_DIR}/ctl --face-only --loop --iterations 140 --poll-ms 100 --wake-at 70 --camera 0 --camera-width 1280 --camera-height 720 --camera-rotate 180 --det-model ${DET_MODEL} --rec-model ${REC_MODEL} --rec-model-fp32 ${REC_MODEL_FP32} --rec-model-i8 ${REC_MODEL_I8} --rec-mode ${REC_MODE} --threshold-fp32 ${THRESHOLD_FP32} --threshold-i8 ${THRESHOLD_I8} --db ${DB_PATH} --debug-face-dir ${DEBUG_DIR}"
}

usage() {
    cat <<'EOF'
usage: board.sh <cmd>

cmd:
  deploy      push binaries and load mydev.ko
  auto        run non-interactive board tests
  sensor      run motion sensor test
  key         run keypad polling test
  ctl-keypad  run controller in keypad-only mode
  ctl-face    run controller in face-match-required mode
  ctl-face-resume  run same-process camera sleep/wake resume test
EOF
}

case "${1:-}" in
    deploy) deploy ;;
    auto) auto_tests ;;
    sensor) sensor_test ;;
    key) key_test ;;
    ctl-keypad) ctl_keypad ;;
    ctl-face) ctl_face ;;
    ctl-face-resume) ctl_face_resume ;;
    *) usage; exit 1 ;;
esac
