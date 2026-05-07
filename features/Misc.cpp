#include <iostream>
#include <chrono>
#include <thread>
#include <Windows.h>
#define NOMINMAX
#include <random>
#include <algorithm>
#include "../features/macros.h"
#include "../util/globals.h"

namespace macros {
    void panicButtonHandler() {
        while (true) {
            if (globals::misc::panicKey && globals::misc::panicKeyBind.key != 0) {
                bool isPressKeyPressed = (GetAsyncKeyState(globals::misc::panicKeyBind.key) & 0x8000) != 0;
                if (isPressKeyPressed && globals::misc::panicKey) {
                    ExitProcess(0);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    int randomNormal(int mean, int stddev) {
        static std::mt19937 generator(std::random_device{}());
        std::normal_distribution<double> distribution(mean, stddev);
        int val = static_cast<int>(distribution(generator));
        if (val < 1) val = 1; // Prevent negative/zero sleeps that cause huge Sleep() or bad behavior
        return val;
    }
    int randomInt(int min, int max) {
        static std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(min, max);
        return distribution(generator);
    }
    void pressKey(int key) {
        if (key == 0) return;
        INPUT ip = { 0 };
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = key;
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));
        int holdTime = randomNormal(15, 6);
        if (holdTime < 5) holdTime = 5;
        if (holdTime > 50) holdTime = 50;
        Sleep(holdTime);
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }
}