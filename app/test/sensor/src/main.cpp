#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dev/dev.h"
#include "run/wake_ctl.h"

namespace {

int ParseInt(const char* value, int fallback)
{
    if (value == NULL) {
        return fallback;
    }
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

}  // namespace

int main(int argc, char* argv[])
{
    int polls = 200;
    int poll_ms = 100;
    int awake_seconds = 5;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--polls" && i + 1 < argc) {
            polls = ParseInt(argv[++i], polls);
        } else if (arg == "--poll-ms" && i + 1 < argc) {
            poll_ms = ParseInt(argv[++i], poll_ms);
        } else if (arg == "--awake-seconds" && i + 1 < argc) {
            awake_seconds = ParseInt(argv[++i], awake_seconds);
        }
    }

    Dev dev;
    std::string error;
    dev.Initialize(&error);
    if (!error.empty()) {
        std::cout << error << std::endl;
    }

    WakeCfg cfg;
    cfg.use_window = false;
    cfg.start_awake = true;
    WakeCtl wake(cfg);
    bool last_motion = false;

    for (int i = 0; i < polls; ++i) {
        const auto now = std::chrono::steady_clock::now();
        wake.Tick(now, false);
        const bool motion = dev.PollMotion();
        if (motion && !last_motion) {
            std::cout << "detected=1" << std::endl;
        } else if (!motion && last_motion) {
            std::cout << "detected=0" << std::endl;
        }
        last_motion = motion;
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    return 0;
}
