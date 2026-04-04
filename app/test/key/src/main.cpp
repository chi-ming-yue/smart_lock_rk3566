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

bool HandlePasswordKey(char key, std::string* input)
{
    static const std::string kPassword = "2580";
    if (key >= '0' && key <= '9') {
        if (input->size() < kPassword.size()) {
            input->push_back(key);
        }
        return false;
    }
    if (key == '*') {
        if (!input->empty()) {
            input->erase(input->size() - 1);
        }
        return false;
    }
    if (key == '#') {
        const bool ok = (*input == kPassword);
        input->clear();
        return ok;
    }
    return false;
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

    int polls = 200;
    int poll_ms = 100;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--polls" && i + 1 < argc) {
            polls = ParseInt(argv[++i], polls);
        } else if (arg == "--poll-ms" && i + 1 < argc) {
            poll_ms = ParseInt(argv[++i], poll_ms);
        }
    }

    std::string input;
    WakeCfg cfg;
    cfg.use_window = false;
    cfg.start_awake = true;
    WakeCtl wake(cfg);
    std::cout << "t_key start: enter physical keys, default password 2580#" << std::endl;
    for (int i = 0; i < polls; ++i) {
        wake.Tick(std::chrono::steady_clock::now(), false);
        const char key = dev.PollKey();
        if (key) {
            std::cout << "key=" << key << std::endl;
            if (HandlePasswordKey(key, &input)) {
                std::cout << "password ok" << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    return 0;
}
