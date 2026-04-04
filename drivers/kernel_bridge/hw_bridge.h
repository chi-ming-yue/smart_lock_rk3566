#ifndef TOTAL_DRIVER_HW_BRIDGE_H
#define TOTAL_DRIVER_HW_BRIDGE_H

#include <string>

class HardwareBridge {
public:
    HardwareBridge();
    ~HardwareBridge();

    bool Initialize(std::string* error);
    bool PollMotion() const;
    char PollKey() const;
    void SetLed(bool enabled);
    void SetOledFaceResult(const std::string& name, float similarity);
    void StartServoSweep();
    void StopServoSweep();

    void SetSimulatedMotion(bool motion);
    void SetSimulatedKey(char key);

private:
    bool initialized_;
    bool simulated_motion_;
    char simulated_key_;
    int fd_;
};

#endif
