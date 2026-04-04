#include "dev/dev.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdio>

namespace {

const char kDevicePath[] = "/dev/test";
const unsigned long kKeyIoctlGetKey = _IOR('k', 3, char);

}  // namespace

Dev::Dev()
    : initialized_(false),
      simulated_motion_(false),
      simulated_key_(0),
      fd_(-1)
{
}

Dev::~Dev()
{
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool Dev::Initialize(std::string* error)
{
    fd_ = open(kDevicePath, O_RDWR);
    if (fd_ < 0) {
        if (error != NULL) {
            *error = "falling back to simulated hardware because /dev/test is unavailable";
        }
        initialized_ = true;
        return true;
    }

    initialized_ = true;
    if (error != NULL) {
        error->clear();
    }
    return true;
}

bool Dev::PollMotion() const
{
    if (fd_ >= 0) {
        const int read_fd = open(kDevicePath, O_RDWR);
        char state = '0';
        const ssize_t ret = read_fd >= 0 ? read(read_fd, &state, 1) : -1;
        if (read_fd >= 0) {
            close(read_fd);
        }
        if (ret == 1) {
            return state == '1';
        }
    }
    return simulated_motion_;
}

char Dev::PollKey() const
{
    if (fd_ >= 0) {
        char key = 0;
        if (ioctl(fd_, kKeyIoctlGetKey, &key) == 0) {
            return key;
        }
    }
    return simulated_key_;
}

void Dev::SetLed(bool enabled)
{
    if (fd_ >= 0) {
        const char value = enabled ? '1' : '0';
        const ssize_t ret = write(fd_, &value, 1);
        (void)ret;
    }
}

void Dev::SetOledFaceResult(const std::string& name, float similarity)
{
    if (fd_ < 0) {
        return;
    }

    const int score_percent = similarity > 0.0f ? static_cast<int>(similarity * 100.0f + 0.5f) : 0;
    char buffer[128];
    const int written = std::snprintf(buffer,
                                      sizeof(buffer),
                                      "O:%s|%d",
                                      name.empty() ? "unknown" : name.c_str(),
                                      score_percent);
    if (written <= 0) {
        return;
    }
    const size_t size = static_cast<size_t>(written) < sizeof(buffer) ? static_cast<size_t>(written) : sizeof(buffer);
    const ssize_t ret = write(fd_, buffer, size);
    (void)ret;
}

void Dev::StartServoSweep()
{
    if (fd_ < 0) {
        return;
    }

    const char value = 'S';
    const ssize_t ret = write(fd_, &value, 1);
    (void)ret;
}

void Dev::StopServoSweep()
{
    if (fd_ < 0) {
        return;
    }

    const char value = 'P';
    const ssize_t ret = write(fd_, &value, 1);
    (void)ret;
}

void Dev::SetSimulatedMotion(bool motion)
{
    simulated_motion_ = motion;
}

void Dev::SetSimulatedKey(char key)
{
    simulated_key_ = key;
}
