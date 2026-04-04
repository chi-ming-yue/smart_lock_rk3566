#include <iostream>

#include "state/state.h"

namespace {

void Print(const char* tag, const Status& status)
{
    std::cout << tag
              << " state=" << ToString(status.state)
              << " wake=" << ToString(status.wake_reason)
              << " unlock=" << ToString(status.unlock_reason)
              << std::endl;
}

}  // namespace

int main()
{
    Ctrl ctrl;
    Print("init", ctrl.status());

    const bool wake_ok = ctrl.WakeFromMotion();
    Print("after_wake", ctrl.status());

    const bool face_ok = ctrl.UnlockFromFace();
    Print("after_face", ctrl.status());

    ctrl.ResetToIdle();
    Print("after_reset", ctrl.status());

    ctrl.WakeFromMotion();
    const bool pwd_ok = ctrl.UnlockFromPassword();
    Print("after_pwd", ctrl.status());

    const bool ok = wake_ok && face_ok && pwd_ok &&
                    ctrl.status().state == SystemState::Unlocked &&
                    ctrl.status().unlock_reason == UnlockReason::PasswordVerified;
    return ok ? 0 : 1;
}
