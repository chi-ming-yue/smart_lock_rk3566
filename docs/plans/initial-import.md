# Initial Import

## Goal

Create a new standalone repository without modifying the original `week3/`, `total/`, and `rk3566_face_rec/` directories.

## Imported Baseline

- Controller entry, state machine, hardware bridge, and keypad test from `total/`
- Kernel module and board-side utilities from `week3/`
- Minimal RKNN/OpenCV face-recognition sources from `rk3566_face_rec/src/`

## Constraints

- Keep original projects untouched
- Preserve current working board behavior before deeper refactoring
- Make the new repository independently buildable

## Deliverables

- New repository structure
- Top-level build for controller binaries
- Imported kernel build
- Basic documentation
