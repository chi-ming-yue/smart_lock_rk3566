#include "state_machine.h"

SmartLockStateMachine::SmartLockStateMachine()
{
    status_.state = SystemState::Idle;
    status_.wake_reason = WakeReason::None;
    status_.unlock_reason = UnlockReason::None;
}

const ControllerStatus& SmartLockStateMachine::status() const
{
    return status_;
}

bool SmartLockStateMachine::WakeFromMotion()
{
    if (status_.state == SystemState::Unlocked) {
        return false;
    }
    status_.state = SystemState::Awake;
    status_.wake_reason = WakeReason::Motion;
    status_.unlock_reason = UnlockReason::None;
    return true;
}

bool SmartLockStateMachine::UnlockFromFace()
{
    if (status_.state != SystemState::Awake) {
        return false;
    }
    status_.state = SystemState::Unlocked;
    status_.unlock_reason = UnlockReason::FaceVerified;
    return true;
}

bool SmartLockStateMachine::UnlockFromPassword()
{
    if (status_.state == SystemState::Unlocked) {
        return false;
    }
    status_.state = SystemState::Unlocked;
    status_.unlock_reason = UnlockReason::PasswordVerified;
    return true;
}

void SmartLockStateMachine::ResetToIdle()
{
    status_.state = SystemState::Idle;
    status_.wake_reason = WakeReason::None;
    status_.unlock_reason = UnlockReason::None;
}
