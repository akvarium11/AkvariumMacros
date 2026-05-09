#include <iostream>
#define NOMINMAX
#include <windows.h>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <random>
#include <algorithm>
#include "macros.h"
#include "../util/globals.h"

namespace macros {
    void doAnchorMacro() {
        if (!globals::features::anchorMacro || globals::features::anchorMacroKey.key == 0) {
            return;
        }

        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;

        while ((GetAsyncKeyState(globals::features::anchorMacroKey.key) & 0x8000) != 0) {
            int baseSwap = globals::features::anchorMacroDelay;
            int stdSwap = std::max(6, baseSwap / 3);
            int swapDelayCool = randomNormal(baseSwap, stdSwap);
            if (swapDelayCool < 5) swapDelayCool = 5;
            Sleep(swapDelayCool);

            ip.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            SendInput(1, &ip, sizeof(INPUT));

            int holdTime = randomNormal(38, 16);
            if (holdTime < 10) holdTime = 10;
            if (holdTime > 110) holdTime = 110;
            Sleep(holdTime);

            ip.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            SendInput(1, &ip, sizeof(INPUT));

            if (randomInt(0, 100) < 25) Sleep(randomInt(2, 6));

            Sleep(10);
        }
    }

    void anchorMacro() {
    while (true) {
        if (globals::unattach) break;

        if (globals::features::anchorMacro && globals::features::anchorMacroKey.key != 0) {
            bool isPressKeyPressed = (GetAsyncKeyState(globals::features::anchorMacroKey.key) & 0x8000) != 0;
            if (isPressKeyPressed) {
                doAnchorMacro();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

    void doCrystalMacro() {
        if (!globals::features::crystalMacro || globals::features::crystalMacroKey.key == 0) {
            return;
        }

        INPUT ip = { 0 };
        ip.type = INPUT_MOUSE;

        while ((GetAsyncKeyState(globals::features::crystalMacroKey.key) & 0x8000) != 0) {
            int baseSwap = globals::features::crystalMacroDelay;
            int stdSwap = std::max(6, baseSwap / 3);
            int swapDelayCool = randomNormal(baseSwap, stdSwap);
            if (swapDelayCool < 5) swapDelayCool = 5;
            Sleep(swapDelayCool);

            ip.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            SendInput(1, &ip, sizeof(INPUT));

            int holdTime = randomNormal(38, 16);
            if (holdTime < 10) holdTime = 10;
            if (holdTime > 110) holdTime = 110;
            Sleep(holdTime);

            ip.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            SendInput(1, &ip, sizeof(INPUT));

            ip.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &ip, sizeof(INPUT));

            int holdTime2 = randomNormal(38, 16);
            if (holdTime2 < 10) holdTime2 = 10;
            if (holdTime2 > 110) holdTime2 = 110;
            Sleep(holdTime2);

            ip.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &ip, sizeof(INPUT));

            if (randomInt(0, 100) < 25) Sleep(randomInt(2, 6));

            Sleep(10);
        }
    }

    void crystalMacro() {
        while (true) {
            if (globals::unattach) break;

            if (globals::features::crystalMacro && globals::features::crystalMacroKey.key != 0) {
                bool isPressKeyPressed = (GetAsyncKeyState(globals::features::crystalMacroKey.key) & 0x8000) != 0;
                if (isPressKeyPressed) {
                    doCrystalMacro();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
