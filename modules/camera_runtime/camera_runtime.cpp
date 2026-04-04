#include "camera_runtime.h"

CameraRuntimeController::CameraRuntimeController(int awake_timeout_seconds)
    : awake_timeout_(std::chrono::seconds(awake_timeout_seconds > 0 ? awake_timeout_seconds : 5)),
      deadline_(std::chrono::steady_clock::time_point::min()),
      awake_(false)
{
}

void CameraRuntimeController::SetAwakeTimeoutSeconds(int awake_timeout_seconds)
{
    awake_timeout_ = std::chrono::seconds(awake_timeout_seconds > 0 ? awake_timeout_seconds : 5);
}

void CameraRuntimeController::StartAwakeWindow(std::chrono::steady_clock::time_point now)
{
    awake_ = true;
    deadline_ = now + awake_timeout_;
}

void CameraRuntimeController::NoteMotion(std::chrono::steady_clock::time_point now)
{
    StartAwakeWindow(now);
}

bool CameraRuntimeController::ShouldSleep(std::chrono::steady_clock::time_point now) const
{
    return awake_ && now >= deadline_;
}

bool CameraRuntimeController::IsAwake() const
{
    return awake_;
}

void CameraRuntimeController::Reset()
{
    awake_ = false;
    deadline_ = std::chrono::steady_clock::time_point::min();
}
