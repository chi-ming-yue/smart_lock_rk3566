#ifndef TOTAL_APP_STATE_MACHINE_H
#define TOTAL_APP_STATE_MACHINE_H

#include "state/status.h"

class Ctrl {
public:
    Ctrl();

    const Status& status() const;

    bool WakeFromMotion();
    bool UnlockFromFace();
    bool UnlockFromPassword();
    void ResetToIdle();

private:
    Status status_;
};

#endif
