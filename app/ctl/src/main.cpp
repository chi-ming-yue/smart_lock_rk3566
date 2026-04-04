#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "run/wake_ctl.h"
#include "dev/dev.h"
#include "pipe/face_pipe.h"
#include "state/state.h"

namespace {

int ParsePositiveInt(const std::string& value, int fallback)
{
    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

float ParsePositiveFloat(const std::string& value, float fallback)
{
    try {
        const float parsed = std::stof(value);
        return parsed > 0.0f ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

std::string ParseRecMode(const std::string& value)
{
    return value == "i8" ? "i8" : "fp32";
}

void PrintStatus(const Status& status)
{
    std::cout << "state=" << ToString(status.state)
              << " wake_reason=" << ToString(status.wake_reason)
              << " unlock_reason=" << ToString(status.unlock_reason)
              << std::endl;
}

bool IsNumericKey(char key)
{
    return key >= '0' && key <= '9';
}

bool HandlePasswordKey(char key, std::string* password_input, Ctrl* controller)
{
    static const std::string kPassword = "2580";

    if (key == 0) {
        return false;
    }

    if (IsNumericKey(key)) {
        if (password_input->size() < kPassword.size()) {
            password_input->push_back(key);
            std::cout << "key=" << key << " password_input=" << *password_input << std::endl;
        } else {
            std::cout << "password buffer full, waiting for confirm" << std::endl;
        }
        return false;
    }

    if (key == '*') {
        if (!password_input->empty()) {
            password_input->pop_back();
        }
        std::cout << "key=delete password_input=" << *password_input << std::endl;
        return false;
    }

    if (key == '#') {
        std::cout << "key=confirm password_input=" << *password_input << std::endl;
        const bool matched = (*password_input == kPassword) && controller->UnlockFromPassword();
        password_input->clear();
        return matched;
    }

    return false;
}

std::vector<int> ParseWakeAt(const std::string& value)
{
    std::vector<int> points;
    std::string token;
    for (std::string::size_type i = 0; i <= value.size(); ++i) {
        const bool split = (i == value.size()) || value[i] == ',';
        if (!split) {
            token.push_back(value[i]);
            continue;
        }
        if (!token.empty()) {
            points.push_back(ParsePositiveInt(token, -1));
            token.clear();
        }
    }
    return points;
}

}  // namespace

int main(int argc, char* argv[])
{
    Ctrl controller;
    Dev hardware;
    FacePipe face;
    FaceCfg face_config;
    std::string error;
    bool run_loop = false;
    int max_iterations = 100;
    int poll_ms = 200;
    int awake_timeout_seconds = 5;
    int unlock_hold_seconds = 3;
    std::string password_input;
    bool keypad_only = false;
    bool face_only = false;
    bool face_match_required = false;
    bool face_wake_pending = false;
    std::vector<int> sim_wake_at;
    WakeCfg wake_cfg;
    wake_cfg.hold_s = awake_timeout_seconds;
    wake_cfg.use_window = !keypad_only;
    wake_cfg.start_awake = keypad_only;
    WakeCtl camera_runtime(wake_cfg);

    std::cout << "total controller booted" << std::endl;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--loop") {
            run_loop = true;
        } else if (arg == "--display") {
            face_config.display_preview = true;
        } else if (arg == "--keypad-only") {
            keypad_only = true;
            run_loop = true;
        } else if (arg == "--face-only") {
            face_only = true;
            run_loop = true;
        } else if (arg == "--face-match-required") {
            face_match_required = true;
        } else if (arg == "--iterations" && i + 1 < argc) {
            max_iterations = ParsePositiveInt(argv[++i], max_iterations);
        } else if (arg == "--poll-ms" && i + 1 < argc) {
            poll_ms = ParsePositiveInt(argv[++i], poll_ms);
        } else if (arg == "--awake-seconds" && i + 1 < argc) {
            awake_timeout_seconds = ParsePositiveInt(argv[++i], awake_timeout_seconds);
        } else if (arg == "--det-model" && i + 1 < argc) {
            face_config.det_model_path = argv[++i];
        } else if (arg == "--rec-model" && i + 1 < argc) {
            face_config.rec_model_path = argv[++i];
        } else if (arg == "--rec-model-fp32" && i + 1 < argc) {
            face_config.rec_model_fp32_path = argv[++i];
        } else if (arg == "--rec-model-i8" && i + 1 < argc) {
            face_config.rec_model_i8_path = argv[++i];
        } else if (arg == "--rec-mode" && i + 1 < argc) {
            face_config.rec_mode = ParseRecMode(argv[++i]);
        } else if (arg == "--rec-error-limit" && i + 1 < argc) {
            face_config.rec_error_limit = ParsePositiveInt(argv[++i], face_config.rec_error_limit);
        } else if (arg == "--db" && i + 1 < argc) {
            face_config.db_path = argv[++i];
        } else if (arg == "--image" && i + 1 < argc) {
            face_config.image_path = argv[++i];
        } else if (arg == "--video" && i + 1 < argc) {
            face_config.video_path = argv[++i];
        } else if (arg == "--debug-frame" && i + 1 < argc) {
            face_config.debug_frame_path = argv[++i];
        } else if (arg == "--debug-face-dir" && i + 1 < argc) {
            face_config.debug_face_dir = argv[++i];
        } else if ((arg == "--camera-index" || arg == "--camera") && i + 1 < argc) {
            face_config.camera_index = ParsePositiveInt(argv[++i], face_config.camera_index);
        } else if (arg == "--camera-width" && i + 1 < argc) {
            face_config.camera_width = ParsePositiveInt(argv[++i], face_config.camera_width);
        } else if (arg == "--camera-height" && i + 1 < argc) {
            face_config.camera_height = ParsePositiveInt(argv[++i], face_config.camera_height);
        } else if (arg == "--camera-rotate" && i + 1 < argc) {
            face_config.camera_rotate = ParsePositiveInt(argv[++i], face_config.camera_rotate);
        } else if (arg == "--camera-mirror" && i + 1 < argc) {
            face_config.camera_mirror = ParsePositiveInt(argv[++i], face_config.camera_mirror) != 0;
        } else if (arg == "--threshold" && i + 1 < argc) {
            face_config.threshold = ParsePositiveFloat(argv[++i], face_config.threshold);
            face_config.th_fp32 = face_config.threshold;
            face_config.th_i8 = face_config.threshold;
        } else if (arg == "--threshold-fp32" && i + 1 < argc) {
            face_config.th_fp32 = ParsePositiveFloat(argv[++i], face_config.th_fp32);
        } else if (arg == "--threshold-i8" && i + 1 < argc) {
            face_config.th_i8 = ParsePositiveFloat(argv[++i], face_config.th_i8);
        } else if (arg == "--motion") {
            hardware.SetSimulatedMotion(true);
            controller.WakeFromMotion();
        } else if (arg == "--wake-at" && i + 1 < argc) {
            sim_wake_at = ParseWakeAt(argv[++i]);
        } else if (arg == "--face") {
            face.SetSimulatedMatch(true, "demo_user", 0.95f);
            if (face.PollRecognition().matched) {
                controller.UnlockFromFace();
            }
        } else if (arg == "--password") {
            controller.UnlockFromPassword();
        } else if (arg == "--key" && i + 1 < argc) {
            const std::string value = argv[++i];
            hardware.SetSimulatedKey(value.empty() ? 0 : value[0]);
        } else if (arg == "--reset") {
            controller.ResetToIdle();
        }
    }

    if (face_only && (face_config.threshold <= 0.0f || face_config.threshold == 0.60f)) {
        face_config.threshold = 0.58f;
        if (face_config.th_fp32 == 0.60f) {
            face_config.th_fp32 = 0.58f;
        }
        if (face_config.th_i8 == 0.60f) {
            face_config.th_i8 = 0.58f;
        }
    }

    wake_cfg.hold_s = awake_timeout_seconds;
    wake_cfg.use_window = !keypad_only;
    wake_cfg.start_awake = keypad_only;
    camera_runtime.Configure(wake_cfg);

    if (!hardware.Initialize(&error)) {
        std::cerr << "hardware init failed: " << error << std::endl;
        return 1;
    }
    if (!keypad_only && !face.Initialize(face_config, &error)) {
        std::cerr << "face init failed: " << error << std::endl;
        return 1;
    }
    if (keypad_only) {
        controller.WakeFromMotion();
        std::cout << "keypad-only mode enabled" << std::endl;
    }
    if (face_only) {
        face_wake_pending = true;
        std::cout << "face-only mode enabled"
                  << " camera_rotate=" << face_config.camera_rotate
                  << " threshold_fp32=" << face_config.th_fp32
                  << " threshold_i8=" << face_config.th_i8
                  << " rec_mode=" << face_config.rec_mode
                  << std::endl;
    }

    if (!run_loop) {
        const Status& status = controller.status();
        hardware.SetLed(status.state == SystemState::Unlocked);
        PrintStatus(status);
        return 0;
    }

    std::cout << "loop mode enabled"
              << " poll_ms=" << poll_ms
              << " awake_timeout_seconds=" << awake_timeout_seconds
              << " unlock_hold_seconds=" << unlock_hold_seconds
              << " iterations=" << max_iterations
              << " keypad_only=" << (keypad_only ? "true" : "false")
              << " face_match_required=" << (face_match_required ? "true" : "false")
              << " threshold_fp32=" << face_config.th_fp32
              << " threshold_i8=" << face_config.th_i8
              << " rec_mode=" << face_config.rec_mode
              << std::endl;

    auto unlock_deadline = std::chrono::steady_clock::time_point::min();
    Status last_status = controller.status();
    PrintStatus(last_status);

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        const auto now = std::chrono::steady_clock::now();
        const bool motion_detected = hardware.PollMotion();
        bool simulated_wake = false;
        bool wake_signal = false;

        for (size_t i = 0; i < sim_wake_at.size(); ++i) {
            if (sim_wake_at[i] == iteration) {
                simulated_wake = true;
                break;
            }
        }

        if (!keypad_only && face_wake_pending) {
            wake_signal = true;
            face_wake_pending = false;
            password_input.clear();
            controller.WakeFromMotion();
            std::cout << "face-only wake -> start face detect" << std::endl;
        }

        if (!keypad_only && !face_only && motion_detected) {
            if (controller.WakeFromMotion()) {
                password_input.clear();
                std::cout << "motion wake -> start face detect" << std::endl;
            }
            wake_signal = true;
        }

        if (!keypad_only && simulated_wake) {
            if (controller.WakeFromMotion()) {
                password_input.clear();
                std::cout << "sim wake -> start face detect" << std::endl;
            }
            wake_signal = true;
        }

        const WakeEvt wake_event = camera_runtime.Tick(now, wake_signal);

        if (!keypad_only && wake_event == WakeEvt::Woke) {
            if (!face.Wake(&error)) {
                std::cerr << "camera wake failed: " << error << std::endl;
                camera_runtime.Sleep();
                controller.ResetToIdle();
            }
        }

        if (!keypad_only &&
            controller.status().state == SystemState::Idle &&
            camera_runtime.Awake()) {
            controller.WakeFromMotion();
        }

        const Status current = controller.status();

        if (current.state == SystemState::Unlocked) {
            if (now >= unlock_deadline) {
                hardware.StopServoSweep();
                hardware.SetLed(false);
                password_input.clear();
                controller.ResetToIdle();
            }
        } else {
            if (!face_only && HandlePasswordKey(hardware.PollKey(), &password_input, &controller)) {
                hardware.SetLed(true);
                hardware.StartServoSweep();
                unlock_deadline = now + std::chrono::seconds(unlock_hold_seconds);
            }

            if (controller.status().state == SystemState::Awake &&
                !keypad_only &&
                camera_runtime.Awake()) {
                const FaceHit result = face.PollRecognition();
                if (result.matched || (!face_match_required && result.detected)) {
                    if (result.matched) {
                        std::cout << "face matched: " << result.name
                                  << " similarity=" << result.similarity
                                  << " rec_mode=" << result.rec_mode
                                  << std::endl;
                        hardware.SetOledFaceResult(result.name, result.similarity);
                    } else {
                        std::cout << "face detected, unlock without recognition match"
                                  << " rec_mode=" << result.rec_mode
                                  << std::endl;
                        hardware.SetOledFaceResult("detected", 0.0f);
                    }
                    if (controller.UnlockFromFace()) {
                        hardware.SetLed(true);
                        hardware.StartServoSweep();
                        unlock_deadline = now + std::chrono::seconds(unlock_hold_seconds);
                    }
                } else {
                    if (result.detected) {
                        std::cout << "face detected but recognition match required" << std::endl;
                        hardware.SetOledFaceResult("unknown", 0.0f);
                    } else {
                        std::cout << "face detect active, no valid face yet" << std::endl;
                    }
                }
            }

            if (!keypad_only &&
                wake_event == WakeEvt::Slept) {
                password_input.clear();
                face.Sleep();
                controller.ResetToIdle();
                std::cout << "camera sleep after awake window" << std::endl;
            }
        }

        const Status& next_status = controller.status();
        hardware.SetLed(next_status.state == SystemState::Unlocked);
        if (next_status.state != last_status.state ||
            next_status.wake_reason != last_status.wake_reason ||
            next_status.unlock_reason != last_status.unlock_reason) {
            PrintStatus(next_status);
            last_status = next_status;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }

    return 0;
}
