#ifndef SMART_LOCK_RK3566_CAMERA_RUNTIME_H
#define SMART_LOCK_RK3566_CAMERA_RUNTIME_H

#include <chrono>

class CamHold {
public:
    explicit CamHold(int awake_timeout_seconds);

    void SetAwakeTimeoutSeconds(int awake_timeout_seconds);
    void StartAwakeWindow(std::chrono::steady_clock::time_point now);
    void NoteMotion(std::chrono::steady_clock::time_point now);
    bool ShouldSleep(std::chrono::steady_clock::time_point now) const;
    bool IsAwake() const;
    void Reset();

private:
    std::chrono::seconds awake_timeout_;
    std::chrono::steady_clock::time_point deadline_;
    bool awake_;
};

#endif
