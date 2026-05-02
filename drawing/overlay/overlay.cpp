#include "overlay.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../imgui/addons/imgui_addons.h"
#include "../../util/config/configsystem.h"
#include "../../util/globals.h"
#include "../../util/notification/notification.h"
#include <Windows.h>
#include <chrono>
#include <string>
#include <vector>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <thread>
#include <cstring>
// Forward declarations for helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
bool fullsc(HWND windowHandle);
void movewindow(HWND hw);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND GetMinecraftWindow() {
    static HWND cachedMcWnd = nullptr;
    static DWORD lastCheck = 0;
    DWORD now = GetTickCount();
    // Only do expensive lookup every ~200ms (huge CPU saver)
    if (cachedMcWnd && (now - lastCheck) < 200) {
        if (IsWindow(cachedMcWnd) && IsWindowVisible(cachedMcWnd)) {
            return cachedMcWnd;
        }
    }
    lastCheck = now;
    // Try exact match first (some versions)
    HWND hWnd = FindWindowA(NULL, "Minecraft");
    if (hWnd) { cachedMcWnd = hWnd; return hWnd; }
    // Try common GLFW class used by Minecraft 1.14+
    hWnd = FindWindowA("GLFW30", NULL);
    if (hWnd) { cachedMcWnd = hWnd; return hWnd; }
    // Try older LWJGL class
    hWnd = FindWindowA("LWJGL", NULL);
    if (hWnd) { cachedMcWnd = hWnd; return hWnd; }
    // Fallback: check if foreground window title contains "Minecraft"
    hWnd = GetForegroundWindow();
    if (hWnd) {
        char title[256] = { 0 };
        GetWindowTextA(hWnd, title, sizeof(title) - 1);
        if (strstr(title, "Minecraft") != nullptr) {
            cachedMcWnd = hWnd;
            return hWnd;
        }
    }
    // Last resort: enumerate windows to find any with "Minecraft" in title
    struct FindData {
        HWND result;
    } findData = { nullptr };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        FindData* data = (FindData*)lParam;
        char title[256] = { 0 };
        GetWindowTextA(hwnd, title, sizeof(title) - 1);
        if (strstr(title, "Minecraft") != nullptr) {
            if (IsWindowVisible(hwnd)) {
                RECT r;
                if (GetWindowRect(hwnd, &r) && (r.right - r.left > 100) && (r.bottom - r.top > 100)) {
                    data->result = hwnd;
                    return FALSE;
                }
            }
        }
        return TRUE;
        }, (LPARAM)&findData);
    if (findData.result) {
        cachedMcWnd = findData.result;
        return findData.result;
    }
    cachedMcWnd = nullptr;
    return nullptr;
}
// Global D3D11 variables
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
namespace overlay {
    static ConfigSystem g_config_system;

    void render_watermark() {
        if (!globals::misc::watermark) return;
        static ImVec2 watermark_pos = ImVec2(10, 10);
        static bool first_time = true;
        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;
        float rounded = style.WindowRounding;
        style.WindowRounding = 0;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
        if (!visible) {
            window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }
        if (first_time || !visible) {
            ImGui::SetNextWindowPos(watermark_pos, ImGuiCond_Always);
        }
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        struct tm local_time;
        localtime_s(&local_time, &time_t);
        char time_str[64];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
        char date_str[64];
        std::strftime(date_str, sizeof(date_str), "%d-%m-%Y", &local_time);
        ImGuiIO& io = ImGui::GetIO();
        int fps = (int)(1.0f / io.DeltaTime);
        std::string watermark_text = "AkvariumMacros";
        std::vector<std::string> info_parts;
        if (first_time && globals::misc::watermarkstuff != nullptr) {
            (*globals::misc::watermarkstuff)[0] = 1;
            (*globals::misc::watermarkstuff)[1] = 1;
        }
        if (globals::misc::watermarkstuff != nullptr) {
            if ((*globals::misc::watermarkstuff)[2] == 1) {
                info_parts.push_back(std::string(date_str));
            }
            if ((*globals::misc::watermarkstuff)[0] == 1) {
                info_parts.push_back("FPS: " + std::to_string(fps));
            }
        }
        if (!info_parts.empty()) {
            watermark_text += " | ";
            for (size_t i = 0; i < info_parts.size(); i++) {
                if (i > 0) watermark_text += " | ";
                watermark_text += info_parts[i];
            }
        }
        ImVec2 text_size = ImGui::CalcTextSize(watermark_text.c_str());
        float padding_x = 3.0f;
        float padding_y = 3.0f;
        float total_width = text_size.x + (padding_x * 2) + 3.0f;
        float total_height = text_size.y + (padding_y * 2) + 1.0f;
        ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
        ImGui::Begin("Watermark", nullptr, window_flags);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (visible) {
            watermark_pos = window_pos;
        }
        ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
        draw->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 2), top_line_color);
        ImVec2 text_pos = ImVec2(window_pos.x + padding_x, window_pos.y + padding_y);
        draw->AddText(text_pos, IM_COL32(255, 255, 255, 255), watermark_text.c_str());
        if (first_time) first_time = false;
        style.WindowRounding = rounded;
        ImGui::End();
    }
    void render_keybind_list() {
        if (!globals::misc::keybinds) return;
        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;
        float rounded = style.WindowRounding;
        style.WindowRounding = 0;
        static ImVec2 keybind_pos = ImVec2(5, 300);
        static bool first_time = true;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
        if (!visible) {
            window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }
        if (first_time || !visible) {
            ImGui::SetNextWindowPos(keybind_pos, ImGuiCond_Always);
            first_time = false;
        }
        std::vector<std::pair<std::string, std::string>> active_keybinds;
        if (globals::features::ADclick && globals::features::ADclickBind.enabled) {
            active_keybinds.push_back({ "AD Clicker", globals::features::ADclickBind.get_key_name() });
        }
        ImVec2 title_size = ImGui::CalcTextSize("Keybinds");
        float content_width = title_size.x;
        for (const auto& bind : active_keybinds) {
            std::string full_text = bind.first + ": " + bind.second;
            ImVec2 text_size = ImGui::CalcTextSize(full_text.c_str());
            content_width = std::max(content_width, text_size.x);
        }
        float padding_x = 3.0f;
        float padding_y = 3.0f;
        float line_spacing = ImGui::GetTextLineHeight() + 2.0f;
        float total_width = content_width + (padding_x * 2) + 1.0f;
        float total_height = padding_y * 2 + title_size.y + 2;
        if (!active_keybinds.empty()) {
            total_height += active_keybinds.size() * line_spacing;
        }
        ImGui::SetNextWindowSize(ImVec2(total_width, total_height), ImGuiCond_Always);
        ImGui::Begin("Keybinds", nullptr, window_flags);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (visible) {
            keybind_pos = window_pos;
        }
        ImU32 text_color = IM_COL32(255, 255, 255, 255);
        ImU32 top_line_color = ImGui::GetColorU32(ImGuiCol_SliderGrab);
        draw->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + 2), top_line_color);
        ImVec2 title_pos = ImVec2(window_pos.x + padding_x, window_pos.y + padding_y);
        draw->AddText(title_pos, text_color, "Keybinds");
        if (!active_keybinds.empty()) {
            float current_y = title_pos.y + title_size.y + 4.0f;
            for (const auto& bind : active_keybinds) {
                std::string full_text = bind.first + ": " + bind.second;
                ImVec2 keybind_text_pos = ImVec2(window_pos.x + padding_x, current_y);
                draw->AddText(keybind_text_pos, text_color, full_text.c_str());
                current_y += line_spacing;
            }
        }
        style.WindowRounding = rounded;
        ImGui::End();
    }
    bool Bind(keybind* keybind, const ImVec2& size_arg = ImVec2(0, 0)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(keybind->get_name().c_str());
        const ImVec2 label_size = ImGui::CalcTextSize(keybind->get_name().c_str(), NULL, true);
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id)) return false;
        bool hovered, held;
        bool Pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
        std::string name = keybind->get_key_name();
        if (keybind->waiting_for_input) {
            name = "[Waiting]";
        }
        if (ImGui::GetIO().MouseClicked[0] && hovered) {
            if (g.ActiveId == id) {
                keybind->waiting_for_input = true;
            }
        }
        else if (ImGui::GetIO().MouseClicked[1] && hovered) {
            ImGui::OpenPopup(keybind->get_name().c_str());
        }
        if (keybind->waiting_for_input) {
            if (ImGui::GetIO().MouseClicked[0] && !hovered) {
                keybind->key = VK_LBUTTON;
                ImGui::ClearActiveID();
                keybind->waiting_for_input = false;
            }
            else {
                if (keybind->set_key()) {
                    ImGui::ClearActiveID();
                    keybind->waiting_for_input = false;
                }
            }
        }
        ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        window->DrawList->AddText(bb.Min + ImVec2(size_arg.x / 2 - ImGui::CalcTextSize(name.c_str()).x / 2, size_arg.y / 2 - ImGui::CalcTextSize(name.c_str()).y / 2), ImGui::GetColorU32(textColor), name.c_str());
        if (ImGui::BeginPopup(keybind->get_name().c_str())) {
            if (ImGui::Selectable("Hold", keybind->type == keybind::HOLD)) keybind->type = keybind::HOLD;
            if (ImGui::Selectable("Toggle", keybind->type == keybind::TOGGLE)) keybind->type = keybind::TOGGLE;
            if (ImGui::Selectable("Always", keybind->type == keybind::ALWAYS)) keybind->type = keybind::ALWAYS;
            ImGui::EndPopup();
        }
        return Pressed;
    }
    void draw_shadowed_text(const char* label) {
        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        ImVec2 pos = ImGui::GetWindowPos();
        float HeaderHeight = ImGui::GetFontSize() + style.WindowPadding.y * 2 + style.ChildBorderSize * 2;
        pos.y = pos.y - 4;
        ImGui::GetWindowDrawList()->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
        ImGui::GetWindowDrawList()->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
        ImGui::GetWindowDrawList()->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(1, -1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
        ImGui::GetWindowDrawList()->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2) + ImVec2(-1, 1), ImGui::GetColorU32(ImGuiCol_TitleBg), label);
        ImGui::GetWindowDrawList()->AddText(pos + style.WindowPadding + ImVec2(0, style.ChildBorderSize * 2), ImGui::GetColorU32(ImGuiCol_Text), label);
        ImGui::SetCursorPosY(HeaderHeight - style.WindowPadding.y + 2);
    }
    void load_interface() {
        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
        ::RegisterClassExW(&wc);
        HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"AkvariumMacros Overlay", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, wc.hInstance, nullptr);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
        MARGINS margin = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margin);
        if (!CreateDeviceD3D(hwnd)) {
            CleanupDeviceD3D();
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return;
        }
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 6;
        style.ChildRounding = 3;
        style.TabRounding = 3;
        style.TabBorderSize = 1;
        style.ItemSpacing = ImVec2(8, 6);
        style.FramePadding = ImVec2(8, 5);
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_WindowShadow] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
        ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
        bool done = false;
        while (!done) {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    done = true;
                    break;
                }
            }
            if (done) break;
            if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                g_ResizeWidth = g_ResizeHeight = 0;
                CreateRenderTarget();
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            // === OPTIMIZED: Only move overlay when Minecraft window actually changed ===
            static RECT lastMcRect = { 0, 0, 0, 0 };
            static HWND lastMcWnd = nullptr;
            HWND mcWnd = GetMinecraftWindow();
            bool mcRectChanged = false;
            if (mcWnd != nullptr) {
                RECT currentRect;
                if (GetWindowRect(mcWnd, &currentRect)) {
                    if (memcmp(&currentRect, &lastMcRect, sizeof(RECT)) != 0 || mcWnd != lastMcWnd) {
                        lastMcRect = currentRect;
                        lastMcWnd = mcWnd;
                        mcRectChanged = true;
                    }
                }
            }
            if (mcRectChanged || overlay::visible) {
                movewindow(hwnd);
            }
            bool isMcFocused = (mcWnd != nullptr) && (GetForegroundWindow() == mcWnd || GetForegroundWindow() == hwnd);
            if (isMcFocused) {
                globals::focused = true;
            }
            else {
                globals::focused = false;
            }
            if (isMcFocused) {
                render_keybind_list();
                render_watermark();
                //if (GetAsyncKeyState(VK_RSHIFT) & 1) overlay::visible = !overlay::visible;
                //if (GetAsyncKeyState(VK_F1) & 1) overlay::visible = !overlay::visible;
                if (GetAsyncKeyState(VK_INSERT) & 1) overlay::visible = !overlay::visible;
                //if (GetAsyncKeyState(VK_HOME) & 1) overlay::visible = !overlay::visible;

                // === Swap/Click Indicators - stacked vertically, centered ===
                {
                    // Per-indicator flash timers (persist across frames via static)
                    static float attributeSwapFlashTimer = 0.0f;
                    static float spearSwapFlashTimer = 0.0f;
                    // Manual edge detection - avoids & 1 bit being consumed by other threads
                    static bool attributeKeyWasDown = false;
                    static bool spearKeyWasDown = false;

                    float dt = ImGui::GetIO().DeltaTime;

                    // Detect attribute swap key press -> start 1s flash
                    if (globals::features::attributeSwapIndicator &&
                        globals::features::attributeSwapKey.key != 0)
                    {
                        bool isDown = (GetAsyncKeyState(globals::features::attributeSwapKey.key) & 0x8000) != 0;
                        if (isDown && !attributeKeyWasDown)
                            attributeSwapFlashTimer = 0.5f;
                        attributeKeyWasDown = isDown;
                    }
                    if (attributeSwapFlashTimer > 0.0f)
                        attributeSwapFlashTimer -= dt;

                    // Detect spear swap key press -> start 1s flash
                    if (globals::features::spearSwapIndicator &&
                        globals::features::spearSwapKey.key != 0)
                    {
                        bool isDown = (GetAsyncKeyState(globals::features::spearSwapKey.key) & 0x8000) != 0;
                        if (isDown && !spearKeyWasDown)
                            spearSwapFlashTimer = 0.5f;
                        spearKeyWasDown = isDown;
                    }
                    if (spearSwapFlashTimer > 0.0f)
                        spearSwapFlashTimer -= dt;

                    ImVec2 screen = ImGui::GetIO().DisplaySize;
                    float baseY = screen.y / 2.0f + 45.0f;
                    float lineH = ImGui::GetTextLineHeight() + 4.0f;
                    int   slot = 0; // vertical slot index

                    // --- AdClick Indicator ---
                    if (globals::features::AdIndicator) {
                        bool enabled = globals::features::ADclick;
                        const char* text = enabled ? "AdClick: ON" : "AdClick: OFF";
                        ImVec4 color = enabled
                            ? globals::features::AdIndicatorColor
                            : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        ImVec2 textSize = ImGui::CalcTextSize(text);
                        ImVec2 pos((screen.x - textSize.x) / 2.0f, baseY + slot * lineH);
                        ImU32  col = ImGui::ColorConvertFloat4ToU32(color);
                        ImGui::GetForegroundDrawList()->AddText(
                            ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 220), text);
                        ImGui::GetForegroundDrawList()->AddText(pos, col, text);
                        slot++;
                    }

                    // --- Attribute Swap Indicator ---
                    if (globals::features::attributeSwapIndicator) {
                        std::string keyName = globals::features::attributeSwapKey.key != 0
                            ? globals::features::attributeSwapKey.get_key_name()
                            : "None";
                        std::string text = "Attribute: " + keyName;
                        // Use user color for 1s after key press; white otherwise
                        ImVec4 color = (attributeSwapFlashTimer > 0.0f)
                            ? globals::features::attributeSwapIndicatorColor
                            : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                        ImVec2 pos((screen.x - textSize.x) / 2.0f, baseY + slot * lineH);
                        ImU32  col = ImGui::ColorConvertFloat4ToU32(color);
                        ImGui::GetForegroundDrawList()->AddText(
                            ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 220), text.c_str());
                        ImGui::GetForegroundDrawList()->AddText(pos, col, text.c_str());
                        slot++;
                    }

                    // --- Spear Swap Indicator ---
                    if (globals::features::spearSwapIndicator) {
                        std::string keyName = globals::features::spearSwapKey.key != 0
                            ? globals::features::spearSwapKey.get_key_name()
                            : "None";
                        std::string text = "Spear: " + keyName;
                        // Use user color for 1s after key press; white otherwise
                        ImVec4 color = (spearSwapFlashTimer > 0.0f)
                            ? globals::features::spearSwapIndicatorColor
                            : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                        ImVec2 pos((screen.x - textSize.x) / 2.0f, baseY + slot * lineH);
                        ImU32  col = ImGui::ColorConvertFloat4ToU32(color);
                        ImGui::GetForegroundDrawList()->AddText(
                            ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 220), text.c_str());
                        ImGui::GetForegroundDrawList()->AddText(pos, col, text.c_str());
                        slot++;
                    }
                }

                if (overlay::visible) {
                    style.WindowShadowSize = 0;
                    style.Colors[ImGuiCol_WindowShadow] = style.Colors[ImGuiCol_SliderGrab];
                    ImGui::SetNextWindowSize(ImVec2(470, 550), ImGuiCond_Once);
                    ImGui::Begin("AkvariumMacros", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    ImVec2 window_pos = ImGui::GetWindowPos();
                    ImVec2 window_size = ImGui::GetWindowSize();
                    ImGui::SetCursorPosY(4);
                    ImAdd::Text(style.Colors[ImGuiCol_Text], "AkvariumMacros");
                    draw->AddShadowRect(ImVec2(window_pos.x, window_pos.y + 21.25), ImVec2(window_pos.x + window_size.x, window_pos.y + 22.25), ImGui::GetColorU32(ImGuiCol_SliderGrab), 15, ImVec2(0, 0), NULL, 0);
                    draw->AddLine(ImVec2(window_pos.x, window_pos.y + 21.25), ImVec2(window_pos.x + window_size.x, window_pos.y + 21.25), ImGui::GetColorU32(ImGuiCol_SliderGrab), 1);
                    ImGui::SetCursorPosY(28.0f);
                    if (ImGui::BeginTabBar("##MainTabs", ImGuiTabBarFlags_Reorderable)) {
                        if (ImGui::BeginTabItem("Macro")) {
                            ImAdd::BeginChild("Macros", ImVec2(ImGui::GetWindowWidth() - 30, 480));
                            draw_shadowed_text("Double Click");

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Auto Double Clicker", &globals::features::ADclick);

                            //ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Toggle Key", &globals::features::ADKeyToggle);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::ADkeyToggleBind, ImVec2(40, 10));

                            ImAdd::CheckBox("Double click on key", &globals::features::ADclickOnKey);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::ADclickBind, ImVec2(40, 10));

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::SliderInt("Click Delay (ms)", &globals::features::ADclickDelay, 0, 100);

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImGui::Separator();

                            ImAdd::Text(ImVec4(1.0, 1.0, 1.0, 1.0), "Inventory slot swap");

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Swap after clicks", &globals::features::swapOnClick);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::SwapOnTargetSlot, ImVec2(40, 10));

                            /* I figured out its kinda impossible to make
                            ImAdd::CheckBox("Swap before clicks", &globals::features::swapBeforeClick);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::SwapBeforeTargetSlot, ImVec2(40, 10)); */

                            ImAdd::CheckBox("Swap between clicks", &globals::features::swapBetweenClicks);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::SwapBetweenTargetSlot, ImVec2(40, 10));

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::SliderInt("Swap delay (ms)", &globals::features::swapDelay, 0, 100);

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Attribute swap", &globals::features::attributeSwap);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::attributeSwapKey, ImVec2(40, 10));

                            ImAdd::Text(ImVec4(1.0, 1.0, 1.0, 1.0), "Slot to swap on");
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::attributeSwapTargetSlot, ImVec2(40, 10));

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::SliderInt("Attribute swap delay (ms)", &globals::features::attributeSwapDelay, 0, 100);

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Spear swap", &globals::features::spearSwap);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::spearSwapKey, ImVec2(40, 10));

                            ImAdd::Text(ImVec4(1.0, 1.0, 1.0, 1.0), "Spear slot");
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::features::spearSwapTargetSlot, ImVec2(40, 10));

                            ImAdd::EndChild();
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Indicators")) {
                            ImAdd::BeginChild("Indicators", ImVec2(ImGui::GetWindowWidth() - 30, 480));
                            draw_shadowed_text("Indicators");

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Auto Dclick Indicator", &globals::features::AdIndicator);
                            ImAdd::Text({ 1.0, 1.0, 1.0, 1.0 }, "Indicator color");
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                            ImAdd::ColorEdit4("##AdIndicatorColor", (float*)&globals::features::AdIndicatorColor);

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Attribute swap indicator", &globals::features::attributeSwapIndicator);
                            ImAdd::Text({ 1.0, 1.0, 1.0, 1.0 }, "Indicator color");
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                            ImAdd::ColorEdit4("##AttrSwapIndicatorColor", (float*)&globals::features::attributeSwapIndicatorColor);

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImGui::Separator();

                            ImGui::Dummy(ImVec2(0.0f, 6.0f));

                            ImAdd::CheckBox("Spear swap indicator", &globals::features::spearSwapIndicator);
                            ImAdd::Text({ 1.0, 1.0, 1.0, 1.0 }, "Indicator color");
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                            ImAdd::ColorEdit4("##SpearSwapIndicatorColor", (float*)&globals::features::spearSwapIndicatorColor);

                            ImAdd::EndChild();
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Misc")) {
                            ImAdd::BeginChild("Misc", ImVec2(ImGui::GetWindowWidth() - 30, 480));
                            draw_shadowed_text("Misc");
                            //ImAdd::CheckBox("Watermark", &globals::misc::watermark);
                            //ImGui::Dummy(ImVec2(0.0f, 6.0f));
                            std::vector<const char*> stuff = { "FPS", "Date" };
                            if (globals::misc::watermarkstuff == nullptr) {
                                globals::misc::watermarkstuff = new std::vector<int>(stuff.size(), 0);
                            }
                            //ImAdd::MultiCombo("Watermark Info", globals::misc::watermarkstuff, stuff);
                            //ImGui::Dummy(ImVec2(0.0f, 6.0f));
                            //ImAdd::CheckBox("VSYNC", &globals::misc::vsync);
                            ImGui::Dummy(ImVec2(0.0f, 6.0f));
                            //ImAdd::CheckBox("Keybind List", &globals::misc::keybinds);
                            //ImGui::Dummy(ImVec2(0.0f, 6.0f));
                            //ImAdd::Combo("Keybind Style", &globals::misc::keybindsstyle, "Dynamic\0Static\0");
                            //ImGui::Dummy(ImVec2(0.0f, 6.0f));
                            ImAdd::CheckBox("Streamproof", &globals::misc::streamproof);
                            ImAdd::Text(ImVec4(1.0, 1.0, 1.0, 1.0), "# Hides gui from Discord / OBS / any recording app");

                            ImGui::Dummy(ImVec2(0.0f, 12.0f));

                            ImAdd::CheckBox("Panic button", &globals::misc::panicKey);
                            ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 60);
                            Bind(&globals::misc::panicKeyBind, ImVec2(40, 10));
                            ImAdd::Text(ImVec4(1.0, 1.0, 1.0, 1.0), "# Instantly exits on key press");

                            ImGui::Dummy(ImVec2(0.0f, 12.0f));

                            if (ImAdd::Button("Exit", ImVec2(ImGui::GetContentRegionAvail().x, 35))) {
                                ExitProcess(0);
                            }
                            ImAdd::EndChild();
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Config")) {
                            g_config_system.render_config_ui(
                                ImGui::GetWindowWidth() - 30,
                                480
                            );
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::End();
                }
            }
            else {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                overlay::visible = false;
            }
            if (overlay::visible) {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
                SetForegroundWindow(hwnd);
            }
            else {
                SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
            }
            if (globals::misc::streamproof) {
                SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            }
            else {
                SetWindowDisplayAffinity(hwnd, WDA_NONE);
            }
            Notifications::Update();
            Notifications::Render();
            ImGui::Render();
            const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (globals::misc::vsync) {
                g_pSwapChain->Present(1, 0);
            }
            else {
                g_pSwapChain->Present(0, 0);
                // === FRAME RATE LIMITER (saves massive FPS) ===
                static auto lastFrameTime = std::chrono::high_resolution_clock::now();
                auto nowTime = std::chrono::high_resolution_clock::now();
                float elapsed = std::chrono::duration<float>(nowTime - lastFrameTime).count();
                float targetFPS = overlay::visible ? 90.0f : 30.0f;
                float targetFrameTime = 1.0f / targetFPS;
                if (elapsed < targetFrameTime) {
                    std::this_thread::sleep_for(std::chrono::duration<float>(targetFrameTime - elapsed));
                }
                lastFrameTime = std::chrono::high_resolution_clock::now();
            }
        }
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
}
// ==================== HELPER FUNCTIONS ====================
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}
void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
bool fullsc(HWND windowHandle) {
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    if (GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
        RECT rect;
        if (GetWindowRect(windowHandle, &rect)) {
            return rect.left == monitorInfo.rcMonitor.left
                && rect.right == monitorInfo.rcMonitor.right
                && rect.top == monitorInfo.rcMonitor.top
                && rect.bottom == monitorInfo.rcMonitor.bottom;
        }
    }
    return false;
}
void movewindow(HWND hw) {
    HWND target = GetMinecraftWindow();
    HWND foregroundWindow = GetForegroundWindow();
    if (target == nullptr || (target != foregroundWindow && hw != foregroundWindow)) {
        MoveWindow(hw, 0, 0, 0, 0, true);
        return;
    }
    RECT rect;
    if (!GetWindowRect(target, &rect)) return;
    int rsize_x = rect.right - rect.left;
    int rsize_y = rect.bottom - rect.top;
    if (fullsc(target)) {
        rsize_x += 16;
        rect.right -= 16;
    }
    else {
        rsize_y -= 39;
        rect.left += 8;
        rect.top += 31;
        rect.right -= 16;
    }
    rsize_x -= 16;
    MoveWindow(hw, rect.left, rect.top, rsize_x, rsize_y, TRUE);
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}