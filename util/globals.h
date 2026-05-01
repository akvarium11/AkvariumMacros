#pragma once
#include "../drawing/overlay/keybind/keybind.h"
#include <vector>
#include <string>

namespace globals
{
    namespace features {
        inline bool ADclick = false;
        inline int ADclickDelay = 30;
        inline bool ADclickOnKey = false;
        inline keybind ADclickBind("ADclickPressKey");
        inline bool ADKeyToggle = false;
        inline keybind ADkeyToggleBind("ADkeyToggleBind");
        // Swap after click
        inline bool swapOnClick = false;
        inline keybind SwapOnTargetSlot("SwapOnTargetSlot");
        // Swap before click
        inline bool swapBeforeClick = false;
        inline keybind SwapBeforeTargetSlot("SwapBeforeTargetSlot");
        // Swap between clicks
        inline bool swapBetweenClicks = false;
        inline keybind SwapBetweenTargetSlot("SwapBetweenTargetSlot");
        inline int swapDelay = 30;
        // Just attribute swap
        inline bool attributeSwap = false;
        inline keybind attributeSwapTargetSlot("attributeSwapTargetSlot");
        inline keybind attributeSwapKey("attributeSwapKey");
        inline int attributeSwapDelay = 30;
        // Just spear swap
        inline bool spearSwap = false;
        inline keybind spearSwapTargetSlot("spearSwapTargetSlot");
        inline keybind spearSwapKey("spearSwapKey");
    }

    namespace misc {
        inline bool watermark = false;
        inline std::vector<int>* watermarkstuff = nullptr;  // 0=FPS, 1=Username, 2=Date
        inline bool vsync = false;
        inline bool keybinds = false;
        inline int keybindsstyle = 0; // 0=Dynamic, 1=Static
        inline bool streamproof = false;
        inline bool colors = false;   // crosshair or similar
        inline bool playerlist = false;
        inline bool explorer = false;
        inline bool targethud = false;
        //
        inline bool panicKey = false;
        inline keybind panicKeyBind("panicKeyBind");
    }

    inline bool focused = false;
    inline bool stop = false;
    inline std::atomic<bool> unattach = false;
}