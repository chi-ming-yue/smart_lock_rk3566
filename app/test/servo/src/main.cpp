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
    int loops = 1;
    int hold_ms = 3500;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--loops" && i + 1 < argc) {
            loops = ParseInt(argv[++i], loops);
        } else if (arg == "--hold-ms" && i + 1 < argc) {
            hold_ms = ParseInt(argv[++i], hold_ms);
        }
    }

    Dev dev;
    std::string error;
    if (!dev.Initialize(&error)) {
        std::cerr << "init failed: " << error << std::endl;
        return 1;
    }
    if (!error.empty()) {
        std::cout << error << std::endl;
    }

    WakeCfg cfg;
    cfg.use_window = false;
    cfg.start_awake = true;
    WakeCtl wake(cfg);
    for (int i = 0; i < loops; ++i) {
        wake.Tick(std::chrono::steady_clock::now(), false);
        std::cout << "servo_open[" << i << "]" << std::endl;
        dev.StartServoSweep();
        std::this_thread::sleep_for(std::chrono::milliseconds(hold_ms));
        std::cout << "servo_stop[" << i << "]" << std::endl;
        dev.StopServoSweep();
    }
    return 0;
}
