#include <cstdlib>
#include <iostream>
#include <string>

#include "dev/dev.h"

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

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--face" && i + 2 < argc) {
            const std::string name = argv[++i];
            const float score = static_cast<float>(std::atof(argv[++i]));
            dev.SetOledFaceResult(name, score);
            std::cout << "oled face result sent: " << name << " " << score << std::endl;
        } else if (arg == "--led" && i + 1 < argc) {
            const bool on = std::string(argv[++i]) == "on";
            dev.SetLed(on);
            std::cout << "oled-linked led state sent: " << (on ? "on" : "off") << std::endl;
        } else if (arg == "--servo" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "open") {
                dev.StartServoSweep();
            } else {
                dev.StopServoSweep();
            }
            std::cout << "oled-linked servo command sent: " << mode << std::endl;
        }
    }
    return 0;
}
