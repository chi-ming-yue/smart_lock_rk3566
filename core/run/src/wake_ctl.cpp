#include "run/wake_ctl.h"

namespace {

std::chrono::seconds ClampHoldSeconds(int hold_s)
{
    return std::chrono::seconds(hold_s > 0 ? hold_s : 5);
}

}  // namespace

WakeCtl::WakeCtl(const WakeCfg& cfg)
    : hold_(ClampHoldSeconds(cfg.hold_s)),
      deadline_(std::chrono::steady_clock::time_point::min()),
      use_window_(cfg.use_window),
      awake_(cfg.start_awake)
{
}

void WakeCtl::Configure(const WakeCfg& cfg)
{
    hold_ = ClampHoldSeconds(cfg.hold_s);
    use_window_ = cfg.use_window;
    awake_ = cfg.start_awake;
    deadline_ = std::chrono::steady_clock::time_point::min();
}

WakeEvt WakeCtl::Tick(std::chrono::steady_clock::time_point now, bool signal)
{
    if (signal) {
        return Wake(now);
    }
    if (use_window_ && awake_ && now >= deadline_) {
        return Sleep();
    }
    return WakeEvt::None;
}

WakeEvt WakeCtl::Wake(std::chrono::steady_clock::time_point now)
{
    const bool woke = !awake_;
    awake_ = true;
    deadline_ = use_window_ ? now + hold_ : std::chrono::steady_clock::time_point::max();
    return woke ? WakeEvt::Woke : WakeEvt::None;
}

WakeEvt WakeCtl::Sleep()
{
    if (!awake_) {
        return WakeEvt::None;
    }
    awake_ = false;
    deadline_ = std::chrono::steady_clock::time_point::min();
    return WakeEvt::Slept;
}

bool WakeCtl::Awake() const
{
    return awake_;
}
