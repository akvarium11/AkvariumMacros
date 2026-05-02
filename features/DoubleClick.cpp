#include <iostream>
#define NOMINMAX
#include <windows.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <algorithm> // for std::max
#include "macros.h"
#include "../util/globals.h"

namespace macros {
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
    void doDoubleClick() {
        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;
        // === CLICK 1 (only in explicit double-click on key mode) ===
        if (!globals::features::ADclick && globals::features::ADclickOnKey) {
            ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &ip, sizeof(INPUT));
            int holdTime1 = randomNormal(26, 12);
            if (holdTime1 < 8) holdTime1 = 8;
            if (holdTime1 > 70) holdTime1 = 70;
            Sleep(holdTime1);
            ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &ip, sizeof(INPUT));
        }
        // === SWAP BETWEEN CLICKS or inter-click delay ===
        if (globals::features::swapBetweenClicks && globals::features::SwapBetweenTargetSlot.key != 0) {
            int baseSwap = globals::features::swapDelay;
            int stdSwap = std::max(6, baseSwap / 3);
            int swapDelayCool = randomNormal(baseSwap, stdSwap);
            if (swapDelayCool < 5) swapDelayCool = 5;
            Sleep(swapDelayCool);
            pressKey(globals::features::SwapBetweenTargetSlot.key);
            // micro jitter after swap key
            Sleep(randomNormal(10, 5));
        }
        else {
            int baseDelay = globals::features::ADclickDelay;
            // Proportional variance for human feel: higher delay = higher absolute variance
            int stddev = std::max(8, baseDelay / 4 + 6);
            int pressTime = randomNormal(baseDelay, stddev);
            if (pressTime < 5) pressTime = 5;
            // Cap outliers to avoid too extreme (anti-cheat might flag insane delays too)
            int maxReasonable = baseDelay * 2 + 50;
            if (pressTime > maxReasonable) pressTime = maxReasonable;
            Sleep(pressTime);
        }
        // === CLICK 2 ===
        ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &ip, sizeof(INPUT));
        int holdTime2 = randomNormal(32, 14);
        if (holdTime2 < 8) holdTime2 = 8;
        if (holdTime2 > 90) holdTime2 = 90;
        Sleep(holdTime2);
        ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &ip, sizeof(INPUT));
        // Small random desync jitter (30% chance)
        if (randomInt(0, 99) < 30) {
            Sleep(randomInt(1, 5));
        }
    }
    void doubleClick() {
        static bool wasToggleKeyPressed = false;
        static bool wasPressKeyPressed = false;
        static int autoClickCounter = 0;  // for occasional human-like pauses
        while (true) {
            if (globals::unattach) break;
            // === 1. TOGGLE WITH KEY ===
            if (globals::features::ADKeyToggle && globals::features::ADkeyToggleBind.key != 0) {
                bool isToggleKeyPressed = (GetAsyncKeyState(globals::features::ADkeyToggleBind.key) & 0x8000) != 0;
                if (isToggleKeyPressed && !wasToggleKeyPressed) {
                    globals::features::ADclick = !globals::features::ADclick;
                }
                wasToggleKeyPressed = isToggleKeyPressed;
            }
            else {
                wasToggleKeyPressed = false;
            }
            // === 2. DOUBLE CLICK ON KEY ===
            if (globals::features::ADclickOnKey && globals::features::ADclickBind.key != 0) {
                bool isPressKeyPressed = (GetAsyncKeyState(globals::features::ADclickBind.key) & 0x8000) != 0;
                if (isPressKeyPressed && !wasPressKeyPressed) {
                    doDoubleClick();
                    if (globals::features::swapOnClick && globals::features::SwapOnTargetSlot.key != 0) {
                        int sd = globals::features::swapDelay;
                        if (sd < 0) sd = 0;
                        Sleep(sd);
                        pressKey(globals::features::SwapOnTargetSlot.key);
                    }
                }
                wasPressKeyPressed = isPressKeyPressed;
            }
            else {
                wasPressKeyPressed = false;
            }
            // === 3. AUTO DOUBLE CLICKER (while holding left mouse) ===
            if (globals::features::ADclick) {
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    doDoubleClick();
                    if (globals::features::swapOnClick && globals::features::SwapOnTargetSlot.key != 0) {
                        int sd = globals::features::swapDelay;
                        if (sd < 0) sd = 0;
                        Sleep(sd);
                        pressKey(globals::features::SwapOnTargetSlot.key);
                    }

                    int baseCycle = globals::features::ADclickDelay;
                    int cycleStd = std::max(10, baseCycle / 3 + 5);
                    int cycleDelay = randomNormal(baseCycle + 8, cycleStd);
                    if (cycleDelay < 1) cycleDelay = 1;

                    autoClickCounter++;
                    if (autoClickCounter % 23 == 0 && randomInt(0, 100) < 12) {
                        cycleDelay += randomNormal(280, 90);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(cycleDelay));
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(randomInt(4, 9)));
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(randomInt(35, 65)));
            }
        }
    }

    // #                       #
    // ## ATTRIBUTE SWAP PART ##
    void doAttributeSwap() {
        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;
        if (globals::features::attributeSwap && globals::features::attributeSwapTargetSlot.key != 0) {
            int baseSwap = globals::features::attributeSwapDelay;
            int stdSwap = std::max(6, baseSwap / 3);
            int swapDelayCool = randomNormal(baseSwap, stdSwap);
            if (swapDelayCool < 5) swapDelayCool = 5;
            Sleep(swapDelayCool);
            pressKey(globals::features::attributeSwapTargetSlot.key);
            // random micro-delay after key press
            Sleep(randomNormal(11, 5));
            //
            ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &ip, sizeof(INPUT));
            int holdTime2 = randomNormal(38, 16);
            if (holdTime2 < 10) holdTime2 = 10;
            if (holdTime2 > 110) holdTime2 = 110;
            Sleep(holdTime2);
            ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &ip, sizeof(INPUT));
            // tiny post jitter
            if (randomInt(0, 100) < 25) Sleep(randomInt(2, 6));
        }
    }
    void attributeSwap() {
        static bool wasPressKeyPressed = false;
        while (true) {
            if (globals::unattach) break;

            if (globals::features::attributeSwap && globals::features::attributeSwapKey.key != 0) {
                bool isPressKeyPressed = (GetAsyncKeyState(globals::features::attributeSwapKey.key) & 0x8000) != 0;
                if (isPressKeyPressed && !wasPressKeyPressed) {
                    doAttributeSwap();
                }
                wasPressKeyPressed = isPressKeyPressed;
            }
            else {
                wasPressKeyPressed = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

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

    // #                    #
    // ## SPEAR SWAP PART  ##
    void doSpearSwap() {
        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;
        if (globals::features::spearSwap && globals::features::spearSwapTargetSlot.key != 0) {
            ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &ip, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(randomNormal(6, 3)));
            pressKey(globals::features::spearSwapTargetSlot.key);
            int holdTime2 = randomNormal(10, 5);
            if (holdTime2 < 10) holdTime2 = 10;
            Sleep(holdTime2);
            ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &ip, sizeof(INPUT));
            std::this_thread::sleep_for(std::chrono::milliseconds(randomNormal(5, 3)));
        }
    }

    void spearSwap() {
        static bool wasPressKeyPressed = false;
        while (true) {
            if (globals::unattach) break;

            if (globals::features::spearSwap && globals::features::spearSwapKey.key != 0) {
                bool isPressKeyPressed = (GetAsyncKeyState(globals::features::spearSwapKey.key) & 0x8000) != 0;
                if (isPressKeyPressed && !wasPressKeyPressed) {
                    doSpearSwap();
                }
                wasPressKeyPressed = isPressKeyPressed;
            }
            else {
                wasPressKeyPressed = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}