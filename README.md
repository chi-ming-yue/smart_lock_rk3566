# smart_lock_rk3566

RK3566 智能门锁重构仓库。当前目标是先验证现有功能，再在短命名模块树下持续重构。

## Tree

- `app/ctl`: 总控程序
- `app/keyt`: 用户态按键测试
- `core/state`: 状态机与状态类型
- `core/run`: 5 秒保持策略
- `hal/dev`: `/dev/test` 用户态桥接
- `face/pipe`: 人脸检测/识别流水线桥
- `drv/mydev`: 内核模块与板级测试
- `third/rknn_face`: RKNN/OpenCV 相关源码
- `docs`: 树结构、验证矩阵、重构说明

详见 [docs/tree.md](/home/chiming/sd3/smart_lock_rk3566/docs/tree.md)。

## Build

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake
cmake --build build -j
```

产物：

- `build/bin/ctl`
- `build/bin/keyt`
- `build/kmod/mydev.ko`
- `build/kmod/key_test`
- `build/kmod/key_debug_test`
- `build/kmod/key_once`
- `build/kmod/sensor_test`

## Run

总控：

```bash
./build/bin/ctl \
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

严格身份匹配：

```bash
./build/bin/ctl \
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

## Notes

- 传感器唤醒采用 5 秒保持策略；5 秒内再次检测到人则重新计时。
- `drv/mydev` 继续使用 Kbuild/Makefile，顶层 CMake 只做包装调用。
- 现有功能验证项见 [docs/verify.md](/home/chiming/sd3/smart_lock_rk3566/docs/verify.md)。
