# smart_lock_rk3566

RK3566 smart-lock baseline extracted from the working `week3/`, `total/`, and `rk3566_face_rec/` implementations.

## Layout

- `apps/controller/`: top-level controller and state machine
- `drivers/kernel_bridge/`: user-space bridge to `/dev/test`
- `drivers/week3_kernel/`: kernel module and board-side test tools
- `modules/camera_runtime/`: camera awake/sleep lifecycle with 5-second hold policy
- `modules/inference_pipeline/`: face detection and recognition bridge
- `third_party/rk3566_face_rec/src/`: imported RKNN/OpenCV inference sources
- `tests/`: user-space keypad test app
- `docs/`: plans and progress notes

## Current Behavior

- Motion sensor wake does not immediately sleep the camera pipeline.
- Each detected motion event extends the awake window by 5 seconds.
- Only after 5 continuous seconds without a new motion signal does the controller return to idle.
- Password unlock and face unlock share the same unlock execution path.

## Build

### Controller

```bash
cmake -S . -B build-board-key -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566_toolchain.cmake
cmake --build build-board-key -j
```

Main artifacts:

- `build-board-key/total_controller`
- `build-board-key/total_keypad_test`

### Kernel Module And Board Tests

```bash
make -C drivers/week3_kernel
```

Main artifacts:

- `drivers/week3_kernel/build/mydev.ko`
- `drivers/week3_kernel/build/key_test`
- `drivers/week3_kernel/build/key_debug_test`
- `drivers/week3_kernel/build/key_once`
- `drivers/week3_kernel/build/sensor_test`

## Deploy

```bash
adb push drivers/week3_kernel/build/mydev.ko /userdata/mydev.ko
adb push build-board-key/total_controller /userdata/total_controller
adb push drivers/week3_kernel/build/sensor_test /userdata/sensor_test
```

Board-side:

```bash
rmmod mydev
insmod /userdata/mydev.ko
```

## Controller Run Examples

Full controller:

```bash
./total_controller \
  --loop \
  --threshold 0.50 \
  --camera 0 \
  --camera-width 1280 \
  --camera-height 720 \
  --camera-rotate 180 \
  --det-model /userdata/face_rec/yolov5s_face_i8.rknn \
  --rec-model /userdata/face_rec/mobilefacenet_int8.rknn \
  --db /userdata/face_db/face_recognition_rknn.db
```

Require identity match before unlock:

```bash
./total_controller \
  --loop \
  --face-match-required \
  --threshold 0.50 \
  --camera 0 \
  --camera-width 1280 \
  --camera-height 720 \
  --camera-rotate 180 \
  --det-model /userdata/face_rec/yolov5s_face_i8.rknn \
  --rec-model /userdata/face_rec/mobilefacenet_int8.rknn \
  --db /userdata/face_db/face_recognition_rknn.db \
  --debug-face-dir /userdata/face_debug
```

Sensor-only test:

```bash
./sensor_test 100
```

## Notes

- The imported kernel module keeps the current working board behavior, including OLED password/servo status updates and servo auto-return.
- `drivers/kernel_bridge/hw_bridge.cpp` reopens `/dev/test` for motion polling to avoid `lseek` on a character device.
- The 5-second motion hold policy is implemented in `modules/camera_runtime/`.
