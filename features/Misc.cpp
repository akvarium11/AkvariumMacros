#include <iostream>
#include <chrono>
#include <thread>
#include <Windows.h>
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
}