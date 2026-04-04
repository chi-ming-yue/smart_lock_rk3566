#ifndef SMART_LOCK_RK3566_WAKE_CTL_H
#define SMART_LOCK_RK3566_WAKE_CTL_H

#include <chrono>

struct WakeCfg {
    int hold_s;
    bool use_window;
    bool start_awake;

    WakeCfg() : hold_s(5), use_window(true), start_awake(false) {}
};

enum class WakeEvt {
    None,
    Woke,
    Slept
};

class WakeCtl {
public:
    explicit WakeCtl(const WakeCfg& cfg = WakeCfg());

    void Configure(const WakeCfg& cfg);
    WakeEvt Tick(std::chrono::steady_clock::time_point now, bool signal);
    WakeEvt Wake(std::chrono::steady_clock::time_point now);
    WakeEvt Sleep();
    bool Awake() const;

private:
    std::chrono::seconds hold_;
    std::chrono::steady_clock::time_point deadline_;
    bool use_window_;
    bool awake_;
};

#endif
