# Test Order

## 1. 主机侧优先

先初始化资源：

```bash
./scripts/init_res.sh
```

然后完成静态检查和构建：

```bash
find res -maxdepth 3 -type f | sort
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake
cmake --build build -j2
cmake --build build --target mydev -j2
```

说明：

- 当前默认构建输出为板端 `aarch64`，不是主机原生二进制
- 可执行测试默认放到第 2、3 步，通过 `adb` 在板端完成

## 2. ADB 自动，无人工交互

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh deploy
app/test/adb/board.sh auto
```

## 3. ADB + 人工物理交互

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh sensor
app/test/adb/board.sh key
app/test/adb/board.sh ctl-keypad
app/test/adb/board.sh ctl-face
```
