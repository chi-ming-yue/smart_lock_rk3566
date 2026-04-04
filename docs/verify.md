# Verify

## Host Build

| Item | Command | Expected | Result |
|---|---|---|---|
| Configure | `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake` | 配置成功 | TODO |
| User build | `cmake --build build -j2` | `build/bin/*` 生成 | TODO |
| Kmod build | `cmake --build build --target mydev -j2` | `build/kmod/mydev.ko` 生成 | TODO |
| Init res | `./scripts/init_res.sh` | `out/board/*` 生成 | TODO |
| Resource tree | `find res -maxdepth 3 -type f | sort` | 资源文件齐全 | TODO |

## Board

| Item | Command | Expected | Result |
|---|---|---|---|
| Deploy | `${ADB} shell ls /userdata/smart_lock` | 板端目录和资源存在 | TODO |
| Auto | `${ADB} shell /userdata/smart_lock/t_dev --led on --oled demo 0.90 --servo open` | OLED/舵机指令可执行 | TODO |
| Rec fp32 | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 20 --rec-mode fp32 ...` | 默认识别模型可启动 | TODO |
| Rec i8 | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 20 --rec-mode i8 ...` | 手动切 i8 可启动 | TODO |
| Rec fallback | `${ADB} shell /userdata/smart_lock/ctl --face-only --loop --iterations 20 --rec-mode fp32 --rec-model-fp32 /bad/path --rec-model-i8 /userdata/smart_lock/model/rec_i8.rknn ...` | fp32 初始化失败后回退到 i8 | TODO |
| Sensor detect | `${ADB} shell /userdata/smart_lock/t_sensor --polls 100 --poll-ms 100` | 检测到人/恢复空闲 | TODO |
| Sensor 5s hold | `${ADB} shell /userdata/smart_lock/t_sensor --awake-seconds 5` | 5 秒内再次触发会重置计时 | TODO |
| Key scan | `${ADB} shell /userdata/smart_lock/t_key --polls 200 --poll-ms 100` | 单键输出稳定 | TODO |
| Password unlock | `${ADB} shell /userdata/smart_lock/ctl --keypad-only --loop` | 输入 `2580#` 后开锁 | TODO |
| Change password | `${ADB} shell /userdata/smart_lock/ctl --keypad-only --loop` | `ABCD` 改密生效 | TODO |
| OLED text | `${ADB} shell /userdata/smart_lock/t_oled --face zw 0.95` | 显示人脸结果 | TODO |
| OLED linked state | `${ADB} shell /userdata/smart_lock/t_oled --led on --servo open` | 显示联动状态 | TODO |
| Servo | `${ADB} shell /userdata/smart_lock/t_servo --loops 1` | 舵机动作并回位 | TODO |
| Face match | `${ADB} shell /userdata/smart_lock/ctl --loop --face-match-required --det-model /userdata/smart_lock/model/det.rknn --rec-model /userdata/smart_lock/model/rec.rknn --db /userdata/smart_lock/db/face.db --debug-face-dir /userdata/smart_lock/debug` | 识别成功后开锁 | TODO |
| Save match frame | `${ADB} shell /userdata/smart_lock/ctl --loop --face-match-required --det-model /userdata/smart_lock/model/det.rknn --rec-model /userdata/smart_lock/model/rec.rknn --db /userdata/smart_lock/db/face.db --debug-face-dir /userdata/smart_lock/debug` | 保存识别成功帧 | TODO |
