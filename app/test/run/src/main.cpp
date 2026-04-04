#include <chrono>
#include <iostream>

#include "run/wake_ctl.h"

int main()
{
    WakeCfg cfg;
    cfg.hold_s = 5;
    cfg.use_window = true;
    WakeCtl hold(cfg);
    const std::chrono::steady_clock::time_point base;

    const bool woke = hold.Wake(base) == WakeEvt::Woke;
    const bool before_5 = hold.Tick(base + std::chrono::seconds(4), false) == WakeEvt::Slept;
    const bool at_5 = hold.Tick(base + std::chrono::seconds(5), false) == WakeEvt::Slept;

    hold.Wake(base + std::chrono::seconds(3));
    const bool after_reset_4 = hold.Tick(base + std::chrono::seconds(7), false) == WakeEvt::Slept;
    const bool after_reset_5 = hold.Tick(base + std::chrono::seconds(8), false) == WakeEvt::Slept;

    std::cout << "woke=" << woke
              << " before_5=" << before_5
              << " at_5=" << at_5
              << " after_reset_4=" << after_reset_4
              << " after_reset_5=" << after_reset_5
              << std::endl;

    return (woke && !before_5 && at_5 && !after_reset_4 && after_reset_5) ? 0 : 1;
}
