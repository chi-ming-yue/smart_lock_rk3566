# Refactor

## Principles

- 先验证，再继续拆分
- 目录名和目标名尽量短
- 头文件和源码分离
- 每个模块独立 `CMakeLists.txt`
- 顶层只负责公共依赖和 `add_subdirectory()`
- 内核模块继续走 Kbuild，由 CMake 包装

## Current Split

- `app/ctl`: 参数解析和总循环
- `app/test`: 分层测试入口
- `core/state`: 状态机
- `core/run`: 5 秒保持策略
- `hal/dev`: `/dev/test` 访问
- `face/pipe`: 当前人脸桥接
- `third/rknn_face`: 第三方推理源码
- `drv/mydev`: 板级内核驱动

## Next Split

- 从 `face/pipe` 继续拆出 `db`
- 从 `app/ctl` 拆出密码认证逻辑
- 从 `drv/mydev/src/main.c` 拆出命令分发层
