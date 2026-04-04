#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dev/dev.h"
#include "run/cam_hold.h"

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

    CamHold hold(awake_seconds);
    bool last_motion = false;

    for (int i = 0; i < polls; ++i) {
        const auto now = std::chrono::steady_clock::now();
        const bool motion = dev.PollMotion();
        if (motion) {
            hold.NoteMotion(now);
            if (!last_motion) {
                std::cout << "detected=1" << std::endl;
            }
        } else if (hold.ShouldSleep(now)) {
            std::cout << "detected=0 sleep=1" << std::endl;
        }
        last_motion = motion;
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    return 0;
}
