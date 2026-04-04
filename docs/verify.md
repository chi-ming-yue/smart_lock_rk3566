# Verify

## Host Build

| Item | Command | Expected | Result |
|---|---|---|---|
| Configure | `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake` | 配置成功 | PASS |
| User build | `cmake --build build -j2` | `build/bin/*` 生成 | PASS |
| Kmod build | `cmake --build build --target mydev -j2` | `build/kmod/mydev.ko` 生成 | PASS |
| Init res | `./scripts/init_res.sh` | `out/board/*` 生成 | PASS |
| Resource tree | `find res -maxdepth 3 -type f | sort` | 资源文件齐全 | TODO |

## Board

| Item | Command | Expected | Result |
|---|---|---|---|
| Deploy | `${ADB} shell ls /userdata/smart_lock` | 板端目录和资源存在 | PASS |
| Auto | `${ADB} shell /userdata/smart_lock/t_dev --led on --oled demo 0.90 --servo open` | OLED/舵机指令可执行 | PASS |
| Rec fp32 | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 60 --rec-mode fp32 --threshold-fp32 0.58 --threshold-i8 0.50 ...` | 默认识别模型可启动，5 秒后休眠 | PASS |
| Rec i8 | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 60 --rec-mode i8 --threshold-fp32 0.58 --threshold-i8 0.50 ...` | 手动切 i8 可启动，5 秒后休眠 | PASS |
| Rec fallback | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 30 --rec-mode fp32 --rec-model-fp32 /bad/path --rec-model-i8 /userdata/smart_lock/model/rec_i8.rknn --threshold-fp32 0.58 --threshold-i8 0.50 ...` | fp32 初始化失败后回退到 i8 | PASS |
| Sensor detect | `${ADB} shell /userdata/smart_lock/t_sensor --polls 20 --poll-ms 100` | 检测到人/恢复无人 | PASS(if physical trigger present) |
| Camera same-process resume | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 140 --poll-ms 100 --wake-at 70 ...` | 同一进程内休眠后再次唤醒可恢复取帧，且不重新 open 相机 | PASS |
| Key scan | `${ADB} shell /userdata/smart_lock/t_key --polls 200 --poll-ms 100` | 单键输出稳定 | TODO |
| Password unlock | `${ADB} shell /userdata/smart_lock/ctl --keypad-only --loop --iterations 20 --poll-ms 100` | 控制器可启动并等待密码 | PASS(startup) |
| Change password | `${ADB} shell /userdata/smart_lock/ctl --keypad-only --loop` | `ABCD` 改密生效 | TODO |
| OLED text | `${ADB} shell /userdata/smart_lock/t_oled --face zw 0.95` | 显示人脸结果 | TODO |
| OLED linked state | `${ADB} shell /userdata/smart_lock/t_oled --led on --servo open` | 显示联动状态 | TODO |
| Servo | `${ADB} shell /userdata/smart_lock/t_servo --loops 1` | 舵机动作并回位 | PASS |
| Face match | `${ADB} shell /userdata/smart_lock/ctl --loop --face-match-required --det-model /userdata/smart_lock/model/det.rknn --rec-model /userdata/smart_lock/model/rec.rknn --db /userdata/smart_lock/db/face.db --debug-face-dir /userdata/smart_lock/debug` | 识别成功后开锁 | TODO |
| Save match frame | `${ADB} shell /userdata/smart_lock/ctl --loop --face-match-required --det-model /userdata/smart_lock/model/det.rknn --rec-model /userdata/smart_lock/model/rec.rknn --db /userdata/smart_lock/db/face.db --debug-face-dir /userdata/smart_lock/debug` | 保存识别成功帧 | TODO |
