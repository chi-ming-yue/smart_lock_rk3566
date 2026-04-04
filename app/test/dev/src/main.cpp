#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dev/dev.h"

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
    Dev dev;
    std::string error;
    if (!dev.Initialize(&error)) {
        std::cerr << "init failed: " << error << std::endl;
        return 1;
    }
    if (!error.empty()) {
        std::cout << error << std::endl;
    }

    int poll_motion = 0;
    int poll_key = 0;
    int poll_ms = 200;
    int sim_motion = -1;
    char sim_key = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--poll-motion" && i + 1 < argc) {
            poll_motion = ParseInt(argv[++i], 1);
        } else if (arg == "--poll-key" && i + 1 < argc) {
            poll_key = ParseInt(argv[++i], 1);
        } else if (arg == "--poll-ms" && i + 1 < argc) {
            poll_ms = ParseInt(argv[++i], poll_ms);
        } else if (arg == "--sim-motion" && i + 1 < argc) {
            sim_motion = std::atoi(argv[++i]);
        } else if (arg == "--sim-key" && i + 1 < argc) {
            sim_key = argv[++i][0];
        } else if (arg == "--led" && i + 1 < argc) {
            dev.SetLed(std::string(argv[++i]) == "on");
        } else if (arg == "--oled" && i + 2 < argc) {
            const std::string name = argv[++i];
            const float score = static_cast<float>(std::atof(argv[++i]));
            dev.SetOledFaceResult(name, score);
        } else if (arg == "--servo" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "open") {
                dev.StartServoSweep();
            } else if (mode == "stop" || mode == "close") {
                dev.StopServoSweep();
            }
        }
    }

    if (sim_motion >= 0) {
        dev.SetSimulatedMotion(sim_motion != 0);
    }
    if (sim_key != 0) {
        dev.SetSimulatedKey(sim_key);
    }

    for (int i = 0; i < poll_motion; ++i) {
        std::cout << "motion[" << i << "]=" << (dev.PollMotion() ? 1 : 0) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }

    for (int i = 0; i < poll_key; ++i) {
        const char key = dev.PollKey();
        std::cout << "key[" << i << "]=" << (key ? key : '0') << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }

    return 0;
}
