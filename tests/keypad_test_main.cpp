#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

const int kRows[] = {104, 114, 100, 101};
const int kCols[] = {97, 98, 99, 147};
const int kLedGpio = 108;
const int kDebounceSamples = 3;
const int kDebounceDelayMs = 8;
const char kKeyMap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

struct KeySample {
    char key;
    int row;
    int col;
};

bool WriteFile(const std::string& path, const std::string& value)
{
    std::ofstream out(path.c_str());
    if (!out.is_open()) {
        return false;
    }
    out << value;
    return out.good();
}

bool ReadFile(const std::string& path, std::string* value)
{
    std::ifstream in(path.c_str());
    if (!in.is_open()) {
        return false;
    }
    std::getline(in, *value);
    return true;
}

std::string GpioPath(int gpio, const char* leaf)
{
    return std::string("/sys/class/gpio/gpio") + std::to_string(gpio) + "/" + leaf;
}

bool EnsureExported(int gpio)
{
    const std::string gpio_dir = std::string("/sys/class/gpio/gpio") + std::to_string(gpio);
    std::ifstream check(gpio_dir.c_str());
    if (check.good()) {
        return true;
    }
    return WriteFile("/sys/class/gpio/export", std::to_string(gpio));
}

bool ConfigureOutput(int gpio, int value)
{
    if (!EnsureExported(gpio)) {
        return false;
    }
    if (!WriteFile(GpioPath(gpio, "direction"), "out")) {
        return false;
    }
    return WriteFile(GpioPath(gpio, "value"), value ? "1" : "0");
}

bool ConfigureInput(int gpio)
{
    if (!EnsureExported(gpio)) {
        return false;
    }
    return WriteFile(GpioPath(gpio, "direction"), "in");
}

bool WriteGpio(int gpio, int value)
{
    return WriteFile(GpioPath(gpio, "value"), value ? "1" : "0");
}

int ReadGpio(int gpio)
{
    std::string value;
    if (!ReadFile(GpioPath(gpio, "value"), &value) || value.empty()) {
        return -1;
    }
    return value[0] == '0' ? 0 : 1;
}

int ParsePositiveInt(const std::string& value, int fallback)
{
    try {
        const int parsed = std::stoi(value);
        return parsed > 0 ? parsed : fallback;
    } catch (...) {
        return fallback;
    }
}

bool IsNumericKey(char key)
{
    return key >= '0' && key <= '9';
}

KeySample EmptySample()
{
    KeySample sample = {0, -1, -1};
    return sample;
}

KeySample ScanMatrixKeyOnce()
{
    KeySample sample = EmptySample();

    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < 4; ++i) {
            if (!WriteGpio(kRows[i], i == row ? 0 : 1)) {
                return sample;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        for (int col = 0; col < 4; ++col) {
            const int value = ReadGpio(kCols[col]);
            if (value == 0) {
                sample.key = kKeyMap[row][col];
                sample.row = row;
                sample.col = col;
                return sample;
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        WriteGpio(kRows[i], 1);
    }
    return sample;
}

KeySample ScanMatrixKeyStable()
{
    KeySample first = ScanMatrixKeyOnce();
    if (first.key == 0) {
        return first;
    }

    for (int i = 1; i < kDebounceSamples; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kDebounceDelayMs));
        const KeySample next = ScanMatrixKeyOnce();
        if (next.key != first.key || next.row != first.row || next.col != first.col) {
            return EmptySample();
        }
    }

    return first;
}

}  // namespace

int main(int argc, char* argv[])
{
    std::string password_input;
    std::string password = "2580";
    std::string reset_progress;
    int poll_ms = 200;
    bool unlocked = false;
    bool change_password_mode = false;
    char last_key = 0;
    int idle_print_ticks = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--poll-ms" && i + 1 < argc) {
            poll_ms = ParsePositiveInt(argv[++i], poll_ms);
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (!ConfigureOutput(kRows[i], 1)) {
            std::cerr << "failed to configure row gpio " << kRows[i] << std::endl;
            return 1;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (!ConfigureInput(kCols[i])) {
            std::cerr << "failed to configure col gpio " << kCols[i] << std::endl;
            return 1;
        }
    }
    if (!ConfigureOutput(kLedGpio, 0)) {
        std::cerr << "failed to configure led gpio " << kLedGpio << std::endl;
        return 1;
    }

    std::cout << "按键测试程序已启动" << std::endl;
    std::cout << "按键布局：123A / 456B / 789C / *0#D" << std::endl;
    std::cout << "默认密码：2580，确认键：#，删除键：*，修改密码：ABCD 后输入 4 位数字再按 #" << std::endl;
    std::cout << "消抖参数：" << kDebounceSamples << " 次采样，间隔 "
              << kDebounceDelayMs << "ms" << std::endl;

    while (true) {
        const KeySample sample = ScanMatrixKeyStable();
        const char key = sample.key;
        if (key == 0) {
            last_key = 0;
            ++idle_print_ticks;
            if (idle_print_ticks >= 5) {
                std::cout << "当前无按键输入" << std::endl;
                idle_print_ticks = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
            continue;
        }

        if (key == last_key) {
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
            continue;
        }
        last_key = key;
        idle_print_ticks = 0;

        std::cout << "检测到按键：row=" << sample.row
                  << " gpio_row=" << kRows[sample.row]
                  << " col=" << sample.col
                  << " gpio_col=" << kCols[sample.col]
                  << " key=" << key << std::endl;

        if (key >= 'A' && key <= 'D') {
            static const std::string kResetSequence = "ABCD";
            if (key == kResetSequence[reset_progress.size()]) {
                reset_progress.push_back(key);
                std::cout << "重置序列输入进度：" << reset_progress << std::endl;
                if (reset_progress == kResetSequence) {
                    change_password_mode = true;
                    unlocked = false;
                    WriteGpio(kLedGpio, 0);
                    password_input.clear();
                    reset_progress.clear();
                    std::cout << "已进入修改密码模式，请输入 4 位新密码后按 # 保存" << std::endl;
                }
            } else {
                reset_progress = (key == 'A') ? "A" : "";
                std::cout << "重置序列输入进度：" << reset_progress << std::endl;
            }
        } else if (IsNumericKey(key)) {
            reset_progress.clear();
            if (password_input.size() < password.size()) {
                password_input.push_back(key);
            }
            std::cout << "输入按键：" << key
                      << (change_password_mode ? "，当前新密码输入：" : "，当前密码输入：")
                      << password_input << std::endl;
        } else if (key == '*') {
            reset_progress.clear();
            if (!password_input.empty()) {
                password_input.pop_back();
            }
            std::cout << "已删除最后一位"
                      << (change_password_mode ? "，当前新密码输入：" : "，当前密码输入：")
                      << password_input << std::endl;
        } else if (key == '#') {
            reset_progress.clear();
            if (change_password_mode) {
                if (password_input.size() == password.size()) {
                    password = password_input;
                    change_password_mode = false;
                    std::cout << "新密码保存成功：" << password << std::endl;
                } else {
                    std::cout << "新密码长度错误，当前长度为：" << password_input.size() << std::endl;
                }
                password_input.clear();
                WriteGpio(kLedGpio, 0);
            } else {
                std::cout << "确认键按下，当前密码输入：" << password_input << std::endl;
                unlocked = (password_input == password);
                WriteGpio(kLedGpio, unlocked ? 1 : 0);
                if (unlocked) {
                    std::cout << "第二个按键开锁测试没问题：密码验证成功，舵机将会旋转开锁并自动回位" << std::endl;
                } else {
                    std::cout << "密码错误，开锁失败" << std::endl;
                }
                password_input.clear();
            }
        } else {
            reset_progress.clear();
            std::cout << "按键 " << key << " 不参与当前功能" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
}
