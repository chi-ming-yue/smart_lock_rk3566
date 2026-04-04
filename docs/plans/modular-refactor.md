# Modular Refactor

## Target Modules

- `apps/controller/`: orchestration and state transitions
- `drivers/kernel_bridge/`: `/dev/test` bridge
- `drivers/week3_kernel/`: kernel-side board integration
- `modules/camera_runtime/`: wake/sleep lifecycle policy
- `modules/inference_pipeline/`: detection and recognition dispatch
- `modules/face_db/`: future database-only matching split
- `modules/interaction/`: future higher-level device interaction split

## First Refactor Slice

Completed in this baseline:

- Extract camera awake/sleep policy into `modules/camera_runtime/`
- Remove direct awake-deadline bookkeeping from controller main loop
- Fix motion polling bridge to work with the character device semantics of `/dev/test`

## Next Refactor Slices

1. Split database matching out of `face_bridge.cpp`
2. Separate OLED/status composition from hardware bridge operations
3. Introduce explicit unlock executor interface for password and face paths
4. Add board-side and host-side verification scripts under `tests/`
