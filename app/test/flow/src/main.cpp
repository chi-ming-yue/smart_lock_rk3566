#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "dev/dev.h"
#include "pipe/face_pipe.h"
#include "run/cam_hold.h"
#include "state/state.h"

namespace {

bool HandlePasswordKey(char key, std::string* password_input, Ctrl* ctrl)
{
    static const std::string kPassword = "2580";
    if (key >= '0' && key <= '9') {
        if (password_input->size() < kPassword.size()) {
            password_input->push_back(key);
        }
        return false;
    }
    if (key == '*') {
        if (!password_input->empty()) {
            password_input->erase(password_input->size() - 1);
        }
        return false;
    }
    if (key == '#') {
        const bool ok = (*password_input == kPassword) && ctrl->UnlockFromPassword();
        password_input->clear();
        return ok;
    }
    return false;
}

void PrintStatus(const char* tag, const Status& status)
{
    std::cout << tag
              << " state=" << ToString(status.state)
              << " wake=" << ToString(status.wake_reason)
              << " unlock=" << ToString(status.unlock_reason)
              << std::endl;
}

}  // namespace

int main(int argc, char* argv[])
{
    Ctrl ctrl;
    Dev dev;
    FacePipe face;
    CamHold hold(5);
    std::string error;
    std::string password_input;
    bool run_face = false;
    bool face_required = false;
    bool run_password = false;

    dev.Initialize(&error);
    if (!error.empty()) {
        std::cout << error << std::endl;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--motion") {
            dev.SetSimulatedMotion(true);
        } else if (arg == "--face-match" && i + 2 < argc) {
            const std::string name = argv[++i];
            const float score = static_cast<float>(std::atof(argv[++i]));
            face.SetSimulatedMatch(true, name, score);
            run_face = true;
        } else if (arg == "--face-required") {
            face_required = true;
        } else if (arg == "--password" && i + 1 < argc) {
            const std::string seq = argv[++i];
            for (size_t j = 0; j < seq.size(); ++j) {
                if (HandlePasswordKey(seq[j], &password_input, &ctrl)) {
                    run_password = true;
                }
            }
        }
    }

    PrintStatus("init", ctrl.status());
    if (dev.PollMotion() && ctrl.WakeFromMotion()) {
        hold.StartAwakeWindow(std::chrono::steady_clock::time_point());
        PrintStatus("after_motion", ctrl.status());
    }

    if (run_face) {
        const FaceHit hit = face.PollRecognition();
        std::cout << "face detected=" << (hit.detected ? 1 : 0)
                  << " matched=" << (hit.matched ? 1 : 0)
                  << " name=" << hit.name
                  << std::endl;
        if (hit.matched || (!face_required && hit.detected)) {
            ctrl.UnlockFromFace();
            PrintStatus("after_face", ctrl.status());
        }
    }

    if (run_password) {
        PrintStatus("after_password", ctrl.status());
    }

    std::cout << "should_sleep_after_5="
              << hold.ShouldSleep(std::chrono::steady_clock::time_point() + std::chrono::seconds(5))
              << std::endl;
    return 0;
}
