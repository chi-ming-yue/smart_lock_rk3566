#ifndef TOTAL_STATE_H
#define TOTAL_STATE_H

#include <string>

enum class SystemState {
    Idle,
    Awake,
    Unlocked,
};

enum class WakeReason {
    None,
    Motion,
};

enum class UnlockReason {
    None,
    FaceVerified,
    PasswordVerified,
};

struct ControllerStatus {
    SystemState state;
    WakeReason wake_reason;
    UnlockReason unlock_reason;
};

inline const char* ToString(SystemState state)
{
    switch (state) {
    case SystemState::Idle:
        return "idle";
    case SystemState::Awake:
        return "awake";
    case SystemState::Unlocked:
        return "unlocked";
    default:
        return "unknown";
    }
}

inline const char* ToString(WakeReason reason)
{
    switch (reason) {
    case WakeReason::None:
        return "none";
    case WakeReason::Motion:
        return "motion";
    default:
        return "unknown";
    }
}

inline const char* ToString(UnlockReason reason)
{
    switch (reason) {
    case UnlockReason::None:
        return "none";
    case UnlockReason::FaceVerified:
        return "face_verified";
    case UnlockReason::PasswordVerified:
        return "password_verified";
    default:
        return "unknown";
    }
}

#endif
