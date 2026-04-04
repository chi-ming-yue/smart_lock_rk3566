#include "state/state.h"

Ctrl::Ctrl()
{
    status_.state = SystemState::Idle;
    status_.wake_reason = WakeReason::None;
    status_.unlock_reason = UnlockReason::None;
}

const Status& Ctrl::status() const
{
    return status_;
}

bool Ctrl::WakeFromMotion()
{
    if (status_.state == SystemState::Unlocked) {
        return false;
    }
    status_.state = SystemState::Awake;
    status_.wake_reason = WakeReason::Motion;
    status_.unlock_reason = UnlockReason::None;
    return true;
}

bool Ctrl::UnlockFromFace()
{
    if (status_.state != SystemState::Awake) {
        return false;
    }
    status_.state = SystemState::Unlocked;
    status_.unlock_reason = UnlockReason::FaceVerified;
    return true;
}

bool Ctrl::UnlockFromPassword()
{
    if (status_.state == SystemState::Unlocked) {
        return false;
    }
    status_.state = SystemState::Unlocked;
    status_.unlock_reason = UnlockReason::PasswordVerified;
    return true;
}

void Ctrl::ResetToIdle()
{
    status_.state = SystemState::Idle;
    status_.wake_reason = WakeReason::None;
    status_.unlock_reason = UnlockReason::None;
}
