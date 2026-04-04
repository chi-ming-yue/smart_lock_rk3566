# smart_lock_rk3566

RK3566 智能门锁项目。仓库按“模块库 + 分层测试 + 完整系统”组织：

- `app/ctl`：完整系统入口
- `app/test`：分层测试入口
- `core/state`：状态机
- `core/run`：5 秒保持策略
- `hal/dev`：`/dev/test` 用户态桥接
- `face/pipe`：人脸检测/识别/匹配流水线
- `drv/mydev`：内核模块与板级驱动源码
- `third/rknn_face`：RKNN/OpenCV 相关源码
- `res`：默认运行资源

详细结构见 [docs/tree.md](/home/chiming/sd3/smart_lock_rk3566/docs/tree.md)。

## 1. 开发环境

### 主机侧要求

- Linux 或 WSL
- `cmake >= 3.10`
- `make`
- `python3`
- RK3566 交叉编译工具链
- RK3566 内核源码目录

### 板端要求

- 板子可通过 `adb` 连接
- 板上能加载内核模块
- 板端默认部署目录为 `/userdata/smart_lock`
- 模型、数据库和调试目录由仓库脚本自动部署，不要求手工准备外部资源

### ADB

所有板级操作统一通过 `ADB` 变量调用，不在命令里写死 `adb`。

Linux 本地：

```bash
export ADB=adb
```

WSL 调 Windows `adb.exe`：

```bash
export ADB="/mnt/c/Users/<user>/AppData/Local/Android/Sdk/platform-tools/adb.exe"
```

更多说明见 [docs/adb.md](/home/chiming/sd3/smart_lock_rk3566/docs/adb.md)。

## 2. 初始化资源

仓库已内置默认运行资源：

- `res/model/det.rknn`
- `res/model/rec_fp32.rknn`
- `res/model/rec_i8.rknn`
- `res/model/rec.rknn`
- `res/db/face.db`
- `res/sql/face.sql`
- `res/face/zw`
- `res/face/hjj`

先初始化资源：

```bash
./scripts/init_res.sh
```

该脚本会：

1. 校验仓库内默认模型和 SQL 是否存在
2. 如 `res/db/face.db` 缺失，则用 `res/sql/face.sql` 重建
3. 默认选择 `fp32` 识别模型，并同时准备 `i8` 保底模型
4. 生成板端部署目录 `out/board`

生成结果：

- `out/board/model/det.rknn`
- `out/board/model/rec_fp32.rknn`
- `out/board/model/rec_i8.rknn`
- `out/board/model/rec.rknn`
- `out/board/db/face.db`
- `out/board/debug`

默认模式：

```bash
./scripts/init_res.sh
```

手动切到 `i8`：

```bash
REC_MODE=i8 ./scripts/init_res.sh
```

如果只想重建数据库：

```bash
python3 scripts/mk_db.py --sql res/sql/face.sql --out res/db/face.db --force
```

## 3. 构建项目

### 配置

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake
```

如工具链或 sysroot 路径不同，可显式覆盖：

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/rk3566.cmake \
  -DTOOLCHAIN_ROOT=/opt/atk-dlrk356x-toolchain \
  -DSYSROOT=/opt/atk-dlrk356x-toolchain/aarch64-buildroot-linux-gnu/sysroot
```

### 编译用户态程序

```bash
cmake --build build -j2
```

### 编译内核模块

```bash
cmake --build build --target mydev -j2
```

### 主要产物

用户态：

- `build/bin/ctl`
- `build/bin/t_state`
- `build/bin/t_run`
- `build/bin/t_dev`
- `build/bin/t_face`
- `build/bin/t_flow`
- `build/bin/t_key`
- `build/bin/t_sensor`
- `build/bin/t_oled`
- `build/bin/t_servo`

内核态：

- `build/kmod/mydev.ko`

## 4. 主机侧说明

先初始化资源：

```bash
./scripts/init_res.sh
```

当前顶层默认构建产物是板端 `aarch64` 可执行文件，不能直接在 x86 主机上运行。也就是说：

- `build/bin/t_*`
- `build/bin/ctl`

默认都应通过 `adb` 在板端执行。

主机侧当前可直接完成的是：

- 资源初始化
- 构建
- 资源文件检查
- ADB 部署

### 主机侧资源检查

仓库内置两组图片目录：

- `res/face/zw`
- `res/face/hjj`

可以直接确认资源是否齐全：

```bash
find res -maxdepth 3 -type f | sort
```

如果你有单独的主机原生构建配置，`t_face` 可复用同一套资源：

```bash
<host-native-t_face> \
  --det-model res/model/det.rknn \
  --rec-model res/model/rec.rknn \
  --rec-model-fp32 res/model/rec_fp32.rknn \
  --rec-model-i8 res/model/rec_i8.rknn \
  --rec-mode fp32 \
  --threshold-fp32 0.58 \
  --threshold-i8 0.50 \
  --db res/db/face.db \
  --image-dir res/face/zw
```

识别阈值按模型拆分：

- `--threshold-fp32`：`rec_fp32.rknn` 使用
- `--threshold-i8`：`rec_i8.rknn` 使用
- `--threshold`：兼容入口，同时覆盖两者

运行时相机策略统一为“唤醒/休眠”：

- 首次唤醒时打开相机
- 5 秒窗口到期时只休眠采集管线，不销毁资源
- 下次唤醒时从休眠恢复，不重新构建整条相机管线

## 5. 板端部署

所有板级操作默认通过 `app/test/adb/board.sh` 完成。

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh deploy
```

`deploy` 会：

1. 初始化仓库资源并生成 `out/board`
2. 推送 `build/bin` 下的测试程序和 `ctl`
3. 推送 `build/kmod/mydev.ko`
4. 推送默认模型和数据库到 `/userdata/smart_lock`
5. 重新加载模块

等价的手工步骤：

```bash
./scripts/init_res.sh
ADB=${ADB:-adb}
${ADB} shell 'mkdir -p /userdata/smart_lock/model /userdata/smart_lock/db /userdata/smart_lock/debug'
${ADB} push build/bin/ctl /userdata/smart_lock/ctl
${ADB} push build/bin/t_state /userdata/smart_lock/t_state
${ADB} push build/bin/t_run /userdata/smart_lock/t_run
${ADB} push build/bin/t_dev /userdata/smart_lock/t_dev
${ADB} push build/bin/t_face /userdata/smart_lock/t_face
${ADB} push build/bin/t_flow /userdata/smart_lock/t_flow
${ADB} push build/bin/t_key /userdata/smart_lock/t_key
${ADB} push build/bin/t_sensor /userdata/smart_lock/t_sensor
${ADB} push build/bin/t_oled /userdata/smart_lock/t_oled
${ADB} push build/bin/t_servo /userdata/smart_lock/t_servo
${ADB} push build/kmod/mydev.ko /userdata/smart_lock/mydev.ko
${ADB} push out/board/model/det.rknn /userdata/smart_lock/model/det.rknn
${ADB} push out/board/model/rec_fp32.rknn /userdata/smart_lock/model/rec_fp32.rknn
${ADB} push out/board/model/rec_i8.rknn /userdata/smart_lock/model/rec_i8.rknn
${ADB} push out/board/model/rec.rknn /userdata/smart_lock/model/rec.rknn
${ADB} push out/board/db/face.db /userdata/smart_lock/db/face.db
${ADB} shell 'chmod +x /userdata/smart_lock/ctl /userdata/smart_lock/t_*'
${ADB} shell 'rmmod mydev >/dev/null 2>&1 || true'
${ADB} shell 'insmod /userdata/smart_lock/mydev.ko'
```

## 6. 板端测试顺序

按“纯软件 -> ADB 自动 -> ADB + 人工交互”的顺序执行。

### 第一步：ADB 自动测试

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh auto
```

包含：

- `t_dev`
- `t_oled`
- `t_servo`
- 默认资源为 `det=i8 + rec=fp32`

### 第二步：人工物理交互测试

#### 传感器

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh sensor
```

传感器测试现在只验证“有人/无人”信号边沿，不再把 5 秒窗口策略混入传感器程序。5 秒窗口只用于摄像头生命周期。

#### 按键

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh key
```

#### 只测密码链路

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh ctl-keypad
```

#### 测人脸链路

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh ctl-face
```

识别成功帧默认保存到：

- `/userdata/smart_lock/debug`

默认识别模式是 `fp32`，如需手动切到 `i8`：

```bash
REC_MODE=i8 app/test/adb/board.sh deploy
REC_MODE=i8 app/test/adb/board.sh ctl-face
```

#### 同进程相机休眠恢复

```bash
ADB=${ADB:-adb}
app/test/adb/board.sh ctl-face-resume
```

该测试会在同一个 `ctl` 进程中完成：

1. 首次唤醒相机
2. 5 秒后进入休眠
3. 再注入一次唤醒信号
4. 验证相机直接恢复取帧，而不是重新 open

## 7. 板端测试程序说明

- `t_dev`
  - 测 `/dev/test` 基本读写和 LED/OLED/舵机命令桥接
- `t_oled`
  - 测 OLED 文案显示
- `t_servo`
  - 测舵机开锁和回位
- `t_sensor`
  - 测传感器检测边沿
- `t_key`
  - 测按键扫描、密码、改密
- `ctl`
  - 完整系统入口

## 8. 常见问题

### 1. 本地没有 `adb`

使用 `ADB` 变量指向 Windows `adb.exe`，见 [docs/adb.md](/home/chiming/sd3/smart_lock_rk3566/docs/adb.md)。

### 2. 板端提示缺少模型或数据库

先执行：

```bash
app/test/adb/board.sh deploy
```

然后检查板上是否存在：

- `/userdata/smart_lock/model/det.rknn`
- `/userdata/smart_lock/model/rec_fp32.rknn`
- `/userdata/smart_lock/model/rec_i8.rknn`
- `/userdata/smart_lock/model/rec.rknn`
- `/userdata/smart_lock/db/face.db`

### 3. 人脸图片测试跑不起来

先确认资源已初始化：

```bash
./scripts/init_res.sh
```

再确认文件存在：

- `res/model/det.rknn`
- `res/model/rec.rknn`
- `res/db/face.db`
- `res/face/zw`
- `res/face/hjj`

### 4. 模块无法加载

检查：

- 板端内核版本是否匹配
- 是否已有旧版 `mydev` 占用
- `dmesg` 是否有加载报错

## 9. 验证矩阵与补充文档

- 验证矩阵： [docs/verify.md](/home/chiming/sd3/smart_lock_rk3566/docs/verify.md)
- 测试顺序： [docs/test_order.md](/home/chiming/sd3/smart_lock_rk3566/docs/test_order.md)
- ADB 说明： [docs/adb.md](/home/chiming/sd3/smart_lock_rk3566/docs/adb.md)
- 重构说明： [docs/refactor.md](/home/chiming/sd3/smart_lock_rk3566/docs/refactor.md)
