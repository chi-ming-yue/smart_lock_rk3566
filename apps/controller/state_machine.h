#ifndef TOTAL_APP_STATE_MACHINE_H
#define TOTAL_APP_STATE_MACHINE_H

#include "total_state.h"

class SmartLockStateMachine {
public:
    SmartLockStateMachine();

    const ControllerStatus& status() const;

    bool WakeFromMotion();
    bool UnlockFromFace();
    bool UnlockFromPassword();
    void ResetToIdle();

private:
    ControllerStatus status_;
};

#endif
