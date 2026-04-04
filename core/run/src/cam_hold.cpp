#include "run/cam_hold.h"

CamHold::CamHold(int awake_timeout_seconds)
    : awake_timeout_(std::chrono::seconds(awake_timeout_seconds > 0 ? awake_timeout_seconds : 5)),
      deadline_(std::chrono::steady_clock::time_point::min()),
      awake_(false)
{
}

void CamHold::SetAwakeTimeoutSeconds(int awake_timeout_seconds)
{
    awake_timeout_ = std::chrono::seconds(awake_timeout_seconds > 0 ? awake_timeout_seconds : 5);
}

void CamHold::StartAwakeWindow(std::chrono::steady_clock::time_point now)
{
    awake_ = true;
    deadline_ = now + awake_timeout_;
}

void CamHold::NoteMotion(std::chrono::steady_clock::time_point now)
{
    StartAwakeWindow(now);
}

bool CamHold::ShouldSleep(std::chrono::steady_clock::time_point now) const
{
    return awake_ && now >= deadline_;
}

bool CamHold::IsAwake() const
{
    return awake_;
}

void CamHold::Reset()
{
    awake_ = false;
    deadline_ = std::chrono::steady_clock::time_point::min();
}
