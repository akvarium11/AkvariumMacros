#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include "../../drawing/imgui/imgui.h"

#define max
#undef max
#define min
#undef min

enum class NotificationType {
    INFO,
    SUCCESS,
    WARNING,
    EROR
};

enum class NotificationState {
    SLIDING_IN,
    DISPLAYING,
    SLIDING_OUT,
    REMOVING
};

struct Notification {
    std::string message;
    NotificationType type;
    NotificationState state;
    DWORD start_time;
    DWORD display_start_time;
    float duration;
    float alpha;
    float slide_offset;       
    float target_y_pos;     
    float current_y_pos;    
    bool marked_for_removal;
    bool fading_out;

    Notification(const std::string& msg, NotificationType t, float dur = 3.0f)
        : message(msg), type(t), duration(dur), alpha(0.0f), fading_out(false),
        slide_offset(-200.0f), state(NotificationState::SLIDING_IN),
        target_y_pos(0.0f), current_y_pos(0.0f), marked_for_removal(false) {
        start_time = GetTickCount();
        display_start_time = 0;
    }
};

class NotificationSystem {
private:
    std::vector<Notification> notifications;
    const float slide_in_time = 0.4f;    
    const float slide_out_time = 0.4f; 
    const float fade_in_time = 0.3f;
    const float fade_out_time = 0.5f;
    const float notification_height = 30.0f;
    const float notification_width = 150.0f;
    const float padding = 8.0f;
    const float animation_speed = 8.0f;  

    ImVec4 GetNotificationColor(NotificationType type) {
        switch (type) {
        case NotificationType::INFO:    return ImVec4(0.2f, 0.6f, 1.0f, 1.0f); 
        case NotificationType::SUCCESS: return ImVec4(0.2f, 0.8f, 0.2f, 1.0f); 
        case NotificationType::WARNING: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);  
        case NotificationType::EROR:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    const char* GetNotificationIcon(NotificationType type) {
        switch (type) {
        case NotificationType::INFO:    return "i";
        case NotificationType::SUCCESS: return "v";
        case NotificationType::WARNING: return "!";
        case NotificationType::EROR:   return "x";
        default: return "?";
        }
    }

    float EaseOutCubic(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    float EaseInOutCubic(float t) {
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

public:
    void AddNotification(const std::string& message, NotificationType type = NotificationType::INFO, float duration = 3.0f) {
        notifications.emplace_back(message, type, duration);
        UpdateTargetPositions();
    }

    void UpdateTargetPositions() {
        float current_y = padding;
        const float min_width = 100.0f;
        const float max_width = 300.0f;
        const float text_padding = 10.0f;

        for (auto& notification : notifications) {
            if (notification.marked_for_removal) continue;

            notification.target_y_pos = current_y;

            ImVec2 text_size = ImGui::CalcTextSize(notification.message.c_str(), nullptr, false, max_width - text_padding);
            float notification_height = std::max(30.0f, text_size.y + 10.0f);

            current_y += notification_height + padding;
        }
    }

    void Update() {
        DWORD current_time = GetTickCount();
        ImGuiIO& io = ImGui::GetIO();
        float delta_time = io.DeltaTime;

        for (auto it = notifications.begin(); it != notifications.end();) {
            float elapsed = (current_time - it->start_time) / 1000.0f;

            float y_diff = it->target_y_pos - it->current_y_pos;
            if (abs(y_diff) > 0.5f) {
                it->current_y_pos += y_diff * animation_speed * delta_time;
            }
            else {
                it->current_y_pos = it->target_y_pos;
            }

            switch (it->state) {
            case NotificationState::SLIDING_IN: {
                if (elapsed < slide_in_time) {
                    float progress = elapsed / slide_in_time;
                    progress = EaseOutCubic(progress);
                    it->slide_offset = -200.0f * (1.0f - progress);
                    it->alpha = progress;
                }
                else {
                    it->state = NotificationState::DISPLAYING;
                    it->slide_offset = 0.0f;
                    it->alpha = 1.0f;
                    it->display_start_time = current_time;
                }
                break;
            }

            case NotificationState::DISPLAYING: {
                float display_elapsed = (current_time - it->display_start_time) / 1000.0f;

                if (display_elapsed >= it->duration) {
                    it->state = NotificationState::SLIDING_OUT;
                    it->fading_out = true;
                    it->start_time = current_time;
                }
                break;
            }

            case NotificationState::SLIDING_OUT: {
                float slide_out_elapsed = (current_time - it->start_time) / 1000.0f;

                if (slide_out_elapsed < slide_out_time) {
                    float progress = slide_out_elapsed / slide_out_time;
                    progress = EaseInOutCubic(progress);
                    it->slide_offset = -200.0f * progress;
                    it->alpha = 1.0f - progress;
                }
                else {
                    it->marked_for_removal = true;
                    it->state = NotificationState::REMOVING;
                    UpdateTargetPositions();
                }
                break;
            }

            case NotificationState::REMOVING: {
                if (abs(it->current_y_pos - it->target_y_pos) < 0.5f) {
                    it = notifications.erase(it);
                    continue;
                }
                break;
            }
            }

            ++it;
        }
    }

    void Render() {
        if (notifications.empty()) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screen_size = io.DisplaySize;

        const float min_width = 100.0f;
        const float max_width = 300.0f;
        const float text_padding = 10.0f;

        for (size_t i = 0; i < notifications.size(); ++i) {
            auto& notification = notifications[i];

            if (notification.marked_for_removal && notification.alpha <= 0.01f) continue;

            ImVec2 text_size = ImGui::CalcTextSize(notification.message.c_str(), nullptr, false, max_width - text_padding);
            float notification_width = std::max(min_width, std::min(max_width, text_size.x + text_padding));
            float notification_height = std::max(30.0f, text_size.y + 10.0f);

            float x_pos = 5.0f + notification.slide_offset;
            float y_pos = notification.current_y_pos;

            ImVec2 notification_pos(x_pos, y_pos);
            ImVec2 notification_size(notification_width, notification_height);

            char window_name[64];
            sprintf_s(window_name, "##notification_%zu", i);

            ImGui::SetNextWindowPos(notification_pos);
            ImGui::SetNextWindowSize(notification_size);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));

            ImVec4 bg_color = ImVec4(0.12f, 0.12f, 0.12f, 1.0f * notification.alpha);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);

            if (ImGui::Begin(window_name, nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 window_pos = ImGui::GetWindowPos();
                ImVec2 window_size = ImGui::GetWindowSize();

                ImVec4 accent_color = GetNotificationColor(notification.type);
                accent_color.w *= notification.alpha;

                draw_list->AddRectFilled(
                    window_pos,
                    window_pos + window_size,
                    ImGui::ColorConvertFloat4ToU32(bg_color)
                );

                ImVec4 outline_color = ImVec4(0.3f, 0.3f, 0.3f, notification.alpha);
                draw_list->AddRect(
                    window_pos,
                    window_pos + window_size,
                    ImGui::ColorConvertFloat4ToU32(outline_color),
                    0.0f, 0, 1.0f
                );

                if (notification.state == NotificationState::DISPLAYING && notification.display_start_time > 0) {
                    DWORD current_time = GetTickCount();
                    float display_elapsed = (current_time - notification.display_start_time) / 1000.0f;
                    float progress = std::min(1.0f, display_elapsed / notification.duration);

                    float progress_bar_height = 2.0f;
                    float progress_width = window_size.x * progress;

                    ImVec2 progress_start = ImVec2(window_pos.x, window_pos.y + window_size.y - progress_bar_height);
                    ImVec2 progress_end = ImVec2(window_pos.x + progress_width, window_pos.y + window_size.y);

                    ImVec4 progress_color = ImVec4(1.0f, 1.0f, 1.0f, notification.alpha);
                    draw_list->AddRectFilled(
                        progress_start,
                        progress_end,
                        ImGui::ColorConvertFloat4ToU32(progress_color)
                    );
                }

                ImGui::SetCursorPos(ImVec2(5, (window_size.y - text_size.y) * 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, notification.alpha));

                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + notification_width - text_padding);
                ImGui::Text("%s", notification.message.c_str());
                ImGui::PopTextWrapPos();

                ImGui::PopStyleColor();
            }
            ImGui::End();

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
        }
    }
};

static NotificationSystem g_notification_system;

namespace Notifications {
    inline void Info(const std::string& message, float duration = 3.0f) {
        g_notification_system.AddNotification(message, NotificationType::INFO, duration);
    }

    inline void Success(const std::string& message, float duration = 3.0f) {
        g_notification_system.AddNotification(message, NotificationType::SUCCESS, duration);
    }

    inline void Warning(const std::string& message, float duration = 4.0f) {
        g_notification_system.AddNotification(message, NotificationType::WARNING, duration);
    }

    inline void Error(const std::string& message, float duration = 5.0f) {
        g_notification_system.AddNotification(message, NotificationType::EROR, duration);
    }

    inline void Update() {
        g_notification_system.Update();
    }

    inline void Render() {
        g_notification_system.Render();
    }
}