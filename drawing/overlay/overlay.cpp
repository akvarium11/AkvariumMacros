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
#include <iostream>
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

    // Автозагрузка последнего конфига при старте
    static bool config_loaded = [&]() {
        std::string last = g_config_system.load_last_config_name();
        if (!last.empty()) {
            g_config_system.load_config(last);
            std::cout << "[CONFIG] Auto-loaded last config: " << last << "\n";
        }
        return true;
        }();

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
        // === REDESIGNED STYLE - Modern cheat menu aesthetic ===
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 4.0f;
        style.TabBorderSize = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(8, 7);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.FramePadding = ImVec2(10, 6);
        style.WindowPadding = ImVec2(12, 12);
        style.ScrollbarSize = 4.0f;
        style.GrabMinSize = 8.0f;
        style.IndentSpacing = 14.0f;
        style.SeparatorTextBorderSize = 1.0f;

        ImVec4* colors = ImGui::GetStyle().Colors;
        // Base palette - White/Grey, high contrast
        colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.93f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.72f, 0.72f, 0.72f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.30f, 0.40f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
        colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.72f, 0.72f, 0.72f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.35f, 0.35f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.85f, 0.85f, 0.85f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_WindowShadow] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
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
                    // =====================================================
                    // REDESIGNED MAIN MENU - Modern cheat client aesthetic
                    // Sidebar navigation + clean content panel layout
                    // =====================================================
                    static int active_tab = 0; // 0=Macro, 1=Indicators, 2=Misc, 3=Config

                    // Helper lambda: styled section header
                    auto SectionHeader = [](const char* label) {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        float w = ImGui::GetContentRegionAvail().x;
                        // Subtle left accent bar
                        dl->AddRectFilled(p, ImVec2(p.x + 2.0f, p.y + ImGui::GetTextLineHeight()), IM_COL32(200, 200, 200, 200));
                        ImGui::SetCursorScreenPos(ImVec2(p.x + 8.0f, p.y));
                        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.80f, 1.0f), label);
                        // Thin separator line after header
                        ImVec2 sep_p = ImGui::GetCursorScreenPos();
                        dl->AddLine(sep_p, ImVec2(sep_p.x + w, sep_p.y), IM_COL32(40, 40, 40, 255), 1.0f);
                        ImGui::Dummy(ImVec2(0.0f, 6.0f));
                        };

                    // Sidebar nav button helper
                    auto NavButton = [](const char* label, bool selected, ImVec2 size) -> bool {
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        bool hovered, held;
                        ImGuiID id = ImGui::GetID(label);
                        ImRect bb(p, p + size);
                        ImGui::ItemSize(size);
                        if (!ImGui::ItemAdd(bb, id)) return false;
                        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

                        // Background
                        ImU32 bg;
                        if (selected)      bg = IM_COL32(38, 38, 38, 255);
                        else if (hovered)  bg = IM_COL32(26, 26, 26, 255);
                        else               bg = IM_COL32(0, 0, 0, 0);
                        dl->AddRectFilled(bb.Min, bb.Max, bg, 4.0f);

                        // Left accent if selected
                        if (selected) {
                            dl->AddRectFilled(bb.Min, ImVec2(bb.Min.x + 2.0f, bb.Max.y), IM_COL32(210, 210, 210, 255), 1.0f);
                        }

                        // Text centered vertically
                        ImVec2 ts = ImGui::CalcTextSize(label);
                        ImU32 tcol = selected ? IM_COL32(230, 230, 230, 255)
                            : hovered ? IM_COL32(180, 180, 180, 255)
                            : IM_COL32(120, 120, 120, 255);
                        dl->AddText(ImVec2(bb.Min.x + 14.0f, bb.Min.y + (size.y - ts.y) * 0.5f), tcol, label);

                        return pressed;
                        };

                    style.WindowShadowSize = 0;
                    // Fixed window size, taller to accommodate sidebar layout
                    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_Once);
                    ImGui::Begin("AkvariumMacros", nullptr,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    ImVec2 window_pos = ImGui::GetWindowPos();
                    ImVec2 window_size = ImGui::GetWindowSize();

                    // ── HEADER BAR ──────────────────────────────────────────
                    const float HEADER_H = 36.0f;
                    // Header background - slightly lighter than window bg
                    draw->AddRectFilled(
                        window_pos,
                        ImVec2(window_pos.x + window_size.x, window_pos.y + HEADER_H),
                        IM_COL32(13, 13, 13, 255), 8.0f,
                        ImDrawFlags_RoundCornersTop
                    );
                    // Bottom border of header
                    draw->AddLine(
                        ImVec2(window_pos.x, window_pos.y + HEADER_H),
                        ImVec2(window_pos.x + window_size.x, window_pos.y + HEADER_H),
                        IM_COL32(30, 30, 30, 255), 1.0f
                    );
                    // Title text
                    {
                        const char* title = "AkvariumMacros";
                        ImVec2 ts = ImGui::CalcTextSize(title);
                        // Shadow
                        draw->AddText(
                            ImVec2(window_pos.x + 13.0f + 1, window_pos.y + (HEADER_H - ts.y) * 0.5f + 1),
                            IM_COL32(0, 0, 0, 180), title
                        );
                        draw->AddText(
                            ImVec2(window_pos.x + 13.0f, window_pos.y + (HEADER_H - ts.y) * 0.5f),
                            IM_COL32(230, 230, 230, 255), title
                        );
                    }
                    // Small FPS indicator top-right
                    {
                        int fps = (int)(1.0f / ImGui::GetIO().DeltaTime);
                        char fps_buf[16];
                        snprintf(fps_buf, sizeof(fps_buf), "%d fps", fps);
                        ImVec2 ts = ImGui::CalcTextSize(fps_buf);
                        draw->AddText(
                            ImVec2(window_pos.x + window_size.x - ts.x - 12.0f,
                                window_pos.y + (HEADER_H - ts.y) * 0.5f),
                            IM_COL32(70, 70, 70, 255), fps_buf
                        );
                    }

                    // ── BODY LAYOUT ─────────────────────────────────────────
                    const float SIDEBAR_W = 110.0f;
                    const float BODY_Y = HEADER_H + 1.0f;
                    const float BODY_H = window_size.y - BODY_Y;
                    const float CONTENT_X = SIDEBAR_W;
                    const float CONTENT_W = window_size.x - SIDEBAR_W;

                    // Sidebar background
                    draw->AddRectFilled(
                        ImVec2(window_pos.x, window_pos.y + BODY_Y),
                        ImVec2(window_pos.x + SIDEBAR_W, window_pos.y + window_size.y),
                        IM_COL32(11, 11, 11, 255),
                        0.0f
                    );
                    // Sidebar/content divider
                    draw->AddLine(
                        ImVec2(window_pos.x + SIDEBAR_W, window_pos.y + BODY_Y),
                        ImVec2(window_pos.x + SIDEBAR_W, window_pos.y + window_size.y),
                        IM_COL32(28, 28, 28, 255), 1.0f
                    );

                    // ── SIDEBAR NAV ─────────────────────────────────────────
                    ImGui::SetCursorScreenPos(ImVec2(window_pos.x, window_pos.y + BODY_Y + 8.0f));
                    ImGui::BeginGroup();
                    const ImVec2 nav_btn_size = ImVec2(SIDEBAR_W, 34.0f);
                    const char* nav_labels[] = { "Macro", "Indicators", "Misc", "Config" };
                    for (int i = 0; i < 4; i++) {
                        ImGui::SetCursorScreenPos(ImVec2(
                            window_pos.x,
                            window_pos.y + BODY_Y + 8.0f + i * (nav_btn_size.y + 4.0f)
                        ));
                        if (NavButton(nav_labels[i], active_tab == i, nav_btn_size))
                            active_tab = i;
                    }
                    ImGui::EndGroup();

                    // ── CONTENT PANEL ────────────────────────────────────────
                    ImGui::SetCursorScreenPos(
                        ImVec2(window_pos.x + CONTENT_X + 10.0f, window_pos.y + BODY_Y + 10.0f)
                    );

                    // Content child window (scrollable)
                    const float content_inner_w = CONTENT_W - 20.0f;
                    const float content_inner_h = BODY_H - 20.0f;

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    ImGui::BeginChild("##ContentPanel", ImVec2(content_inner_w, content_inner_h),
                        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();

                    // ── TAB 0: MACRO ─────────────────────────────────────────
                    if (active_tab == 0) {
                        ImAdd::BeginChild("Macros", ImVec2(content_inner_w, content_inner_h - 2.0f));

                        SectionHeader("Double Click");

                        ImAdd::CheckBox("Auto Double Clicker", &globals::features::ADclick);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));

                        ImAdd::CheckBox("Toggle Key", &globals::features::ADKeyToggle);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::ADkeyToggleBind, ImVec2(44, 14));

                        ImAdd::CheckBox("Double click on key", &globals::features::ADclickOnKey);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::ADclickBind, ImVec2(44, 14));

                        ImGui::Dummy(ImVec2(0.0f, 6.0f));
                        ImAdd::SliderInt("Click Delay (ms)", &globals::features::ADclickDelay, 0, 100);
                        ImGui::Dummy(ImVec2(0.0f, 10.0f));

                        SectionHeader("Inventory Slot Swap");

                        ImAdd::CheckBox("Swap after clicks", &globals::features::swapOnClick);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::SwapOnTargetSlot, ImVec2(44, 14));

                        ImAdd::CheckBox("Swap between clicks", &globals::features::swapBetweenClicks);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::SwapBetweenTargetSlot, ImVec2(44, 14));

                        ImGui::Dummy(ImVec2(0.0f, 6.0f));
                        ImAdd::SliderInt("Swap delay (ms)", &globals::features::swapDelay, 0, 100);
                        ImGui::Dummy(ImVec2(0.0f, 10.0f));

                        SectionHeader("Attribute Swap");

                        ImAdd::CheckBox("Attribute swap", &globals::features::attributeSwap);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::attributeSwapKey, ImVec2(44, 14));

                        ImAdd::Text(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Slot to swap on");
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::attributeSwapTargetSlot, ImVec2(44, 14));

                        ImGui::Dummy(ImVec2(0.0f, 6.0f));
                        ImAdd::SliderInt("Attribute swap delay (ms)", &globals::features::attributeSwapDelay, 0, 100);
                        ImGui::Dummy(ImVec2(0.0f, 10.0f));

                        SectionHeader("Spear Swap");

                        ImAdd::CheckBox("Spear swap", &globals::features::spearSwap);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::spearSwapKey, ImVec2(44, 14));

                        ImAdd::Text(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Spear slot");
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::features::spearSwapTargetSlot, ImVec2(44, 14));

                        ImAdd::EndChild();
                    }

                    // ── TAB 1: INDICATORS ────────────────────────────────────
                    else if (active_tab == 1) {
                        ImAdd::BeginChild("Indicators", ImVec2(content_inner_w, content_inner_h - 2.0f));

                        SectionHeader("Click Indicators");

                        ImAdd::CheckBox("Auto Dclick Indicator", &globals::features::AdIndicator);
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        ImAdd::Text(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Indicator color");
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                        ImAdd::ColorEdit4("##AdIndicatorColor", (float*)&globals::features::AdIndicatorColor);

                        ImGui::Dummy(ImVec2(0.0f, 10.0f));
                        SectionHeader("Swap Indicators");

                        ImAdd::CheckBox("Attribute swap indicator", &globals::features::attributeSwapIndicator);
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        ImAdd::Text(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Indicator color");
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                        ImAdd::ColorEdit4("##AttrSwapIndicatorColor", (float*)&globals::features::attributeSwapIndicatorColor);

                        ImGui::Dummy(ImVec2(0.0f, 10.0f));

                        ImAdd::CheckBox("Spear swap indicator", &globals::features::spearSwapIndicator);
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                        ImAdd::Text(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "Indicator color");
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 20);
                        ImAdd::ColorEdit4("##SpearSwapIndicatorColor", (float*)&globals::features::spearSwapIndicatorColor);

                        ImAdd::EndChild();
                    }

                    // ── TAB 2: MISC ──────────────────────────────────────────
                    else if (active_tab == 2) {
                        ImAdd::BeginChild("Misc", ImVec2(content_inner_w, content_inner_h - 2.0f));

                        SectionHeader("General");

                        std::vector<const char*> stuff = { "FPS", "Date" };
                        if (globals::misc::watermarkstuff == nullptr) {
                            globals::misc::watermarkstuff = new std::vector<int>(stuff.size(), 0);
                        }

                        ImAdd::CheckBox("Streamproof", &globals::misc::streamproof);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                        ImAdd::Text(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "# Hides gui from Discord / OBS / any recording app");

                        ImGui::Dummy(ImVec2(0.0f, 10.0f));
                        SectionHeader("Safety");

                        ImAdd::CheckBox("Panic button", &globals::misc::panicKey);
                        ImGui::SameLine(ImGui::GetWindowWidth() / 2 - 50);
                        Bind(&globals::misc::panicKeyBind, ImVec2(44, 14));
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                        ImAdd::Text(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "# Instantly exits on key press");

                        ImGui::Dummy(ImVec2(0.0f, 16.0f));

                        // Exit button full width
                        if (ImAdd::Button("Exit", ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
                            ExitProcess(0);
                        }

                        ImAdd::EndChild();
                    }

                    // ── TAB 3: CONFIG ─────────────────────────────────────────
                    else if (active_tab == 3) {
                        g_config_system.render_config_ui(
                            content_inner_w,
                            content_inner_h - 2.0f
                        );
                    }

                    ImGui::EndChild(); // ##ContentPanel
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