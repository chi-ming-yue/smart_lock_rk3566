#include <chrono>
#include <iostream>

#include "run/cam_hold.h"

int main()
{
    CamHold hold(5);
    const std::chrono::steady_clock::time_point base;

    hold.StartAwakeWindow(base);
    const bool before_5 = hold.ShouldSleep(base + std::chrono::seconds(4));
    const bool at_5 = hold.ShouldSleep(base + std::chrono::seconds(5));

    hold.NoteMotion(base + std::chrono::seconds(3));
    const bool after_reset_4 = hold.ShouldSleep(base + std::chrono::seconds(7));
    const bool after_reset_5 = hold.ShouldSleep(base + std::chrono::seconds(8));

    std::cout << "before_5=" << before_5
              << " at_5=" << at_5
              << " after_reset_4=" << after_reset_4
              << " after_reset_5=" << after_reset_5
              << std::endl;

    return (!before_5 && at_5 && !after_reset_4 && after_reset_5) ? 0 : 1;
}
