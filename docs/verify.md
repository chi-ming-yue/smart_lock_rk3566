# Verify

## Host Build

| Item | Command | Expected |
|---|---|---|
| ctl build | `cmake --build build -j` | `build/bin/ctl` |
| keyt build | `cmake --build build -j` | `build/bin/keyt` |
| mydev build | `cmake --build build --target mydev` | `build/kmod/mydev.ko` |

## Board

| Item | Command | Expected | Result |
|---|---|---|---|
| Sensor detect | `sensor_test 100` | 检测到人/恢复空闲 | TODO |
| Sensor 5s hold | `ctl --loop` | 5 秒内触发重置计时 | TODO |
| Key scan | `key_debug_test` | 单键输出稳定 | TODO |
| Password unlock | 输入 `2580#` | 舵机开锁并回位 | TODO |
| Change password | `ABCD` + 新密码 | 新密码生效 | TODO |
| OLED sensor | 触发传感器 | 显示状态变化 | TODO |
| OLED pwd | 密码正确/错误 | 显示密码状态 | TODO |
| OLED servo | 开锁/回位 | 显示舵机状态 | TODO |
| Face detect | `ctl --loop` | 检测到人脸 | TODO |
| Face match | `--face-match-required` | 识别成功后开锁 | TODO |
| Save match frame | `--debug-face-dir` | 保存识别成功帧 | TODO |
