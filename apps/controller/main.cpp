#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "camera_runtime.h"
#include "hw_bridge.h"
#include "face_bridge.h"
#include "state_machine.h"

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

void PrintStatus(const ControllerStatus& status)
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

bool HandlePasswordKey(char key, std::string* password_input, SmartLockStateMachine* controller)
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

}  // namespace

int main(int argc, char* argv[])
{
    SmartLockStateMachine controller;
    HardwareBridge hardware;
    FaceBridge face;
    FaceBridgeConfig face_config;
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
    CameraRuntimeController camera_runtime(awake_timeout_seconds);

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
            camera_runtime.SetAwakeTimeoutSeconds(awake_timeout_seconds);
        } else if (arg == "--det-model" && i + 1 < argc) {
            face_config.det_model_path = argv[++i];
        } else if (arg == "--rec-model" && i + 1 < argc) {
            face_config.rec_model_path = argv[++i];
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
        } else if (arg == "--motion") {
            hardware.SetSimulatedMotion(true);
            controller.WakeFromMotion();
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
        if (face_config.threshold <= 0.0f || face_config.threshold == 0.60f) {
            face_config.threshold = 0.58f;
        }
        controller.WakeFromMotion();
        std::cout << "face-only mode enabled"
                  << " camera_rotate=" << face_config.camera_rotate
                  << " threshold=" << face_config.threshold
                  << std::endl;
    }

    if (!run_loop) {
        const ControllerStatus& status = controller.status();
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
              << std::endl;

    auto unlock_deadline = std::chrono::steady_clock::time_point::min();
    ControllerStatus last_status = controller.status();
    if (face_only && controller.WakeFromMotion()) {
        camera_runtime.StartAwakeWindow(std::chrono::steady_clock::now());
    }
    PrintStatus(last_status);

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        const auto now = std::chrono::steady_clock::now();
        const bool motion_detected = hardware.PollMotion();
        const ControllerStatus current = controller.status();

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

            const ControllerStatus after_password = controller.status();
            if (after_password.state == SystemState::Idle && motion_detected && !keypad_only) {
                if (controller.WakeFromMotion()) {
                    password_input.clear();
                    std::cout << "motion wake -> start face detect" << std::endl;
                }
                camera_runtime.NoteMotion(now);
            }

            if (controller.status().state == SystemState::Awake && !keypad_only) {
                if (motion_detected || face_only) {
                    camera_runtime.NoteMotion(now);
                }
                const FaceMatchResult result = face.PollRecognition();
                if (result.matched || (!face_match_required && result.detected)) {
                    if (result.matched) {
                        std::cout << "face matched: " << result.name
                                  << " similarity=" << result.similarity << std::endl;
                        hardware.SetOledFaceResult(result.name, result.similarity);
                    } else {
                        std::cout << "face detected, unlock without recognition match" << std::endl;
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

            if (controller.status().state == SystemState::Awake &&
                camera_runtime.ShouldSleep(now)) {
                password_input.clear();
                controller.ResetToIdle();
                camera_runtime.Reset();
            }
        }

        const ControllerStatus& next_status = controller.status();
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
