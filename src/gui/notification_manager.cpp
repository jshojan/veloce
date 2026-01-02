#include "notification_manager.hpp"

#include <imgui.h>
#include <algorithm>

namespace emu {

NotificationManager::NotificationManager() = default;
NotificationManager::~NotificationManager() = default;

void NotificationManager::add(const std::string& message,
                               NotificationType type,
                               float duration_seconds) {
    // Limit number of notifications
    if (m_notifications.size() >= MAX_NOTIFICATIONS) {
        m_notifications.erase(m_notifications.begin());
    }

    Notification notif;
    notif.message = message;
    notif.type = type;
    notif.created_at = std::chrono::steady_clock::now();
    notif.duration_seconds = duration_seconds;

    m_notifications.push_back(notif);
}

void NotificationManager::info(const std::string& message, float duration) {
    add(message, NotificationType::Info, duration);
}

void NotificationManager::success(const std::string& message, float duration) {
    add(message, NotificationType::Success, duration);
}

void NotificationManager::warning(const std::string& message, float duration) {
    add(message, NotificationType::Warning, duration);
}

void NotificationManager::error(const std::string& message, float duration) {
    add(message, NotificationType::Error, duration);
}

void NotificationManager::render() {
    if (m_notifications.empty()) {
        return;
    }

    cleanup_expired();

    // Get main viewport for positioning
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;

    // Position notifications in top-right corner, below menu bar
    float y_offset = work_pos.y + 40.0f;  // Account for menu bar

    auto now = std::chrono::steady_clock::now();

    for (size_t i = 0; i < m_notifications.size(); ++i) {
        const auto& notif = m_notifications[i];

        // Calculate elapsed time and alpha for fade effect
        float elapsed = std::chrono::duration<float>(now - notif.created_at).count();
        float remaining = notif.duration_seconds - elapsed;

        // Calculate alpha (fade out near the end)
        float alpha = 1.0f;
        if (remaining < NOTIFICATION_FADE_TIME) {
            alpha = remaining / NOTIFICATION_FADE_TIME;
        }
        // Also fade in at the start
        if (elapsed < NOTIFICATION_FADE_TIME) {
            alpha = std::min(alpha, elapsed / NOTIFICATION_FADE_TIME);
        }

        // Determine color based on type
        ImVec4 bg_color;
        ImVec4 text_color;
        switch (notif.type) {
            case NotificationType::Success:
                bg_color = ImVec4(0.1f, 0.4f, 0.1f, 0.9f * alpha);
                text_color = ImVec4(0.7f, 1.0f, 0.7f, alpha);
                break;
            case NotificationType::Warning:
                bg_color = ImVec4(0.5f, 0.4f, 0.0f, 0.9f * alpha);
                text_color = ImVec4(1.0f, 0.9f, 0.5f, alpha);
                break;
            case NotificationType::Error:
                bg_color = ImVec4(0.5f, 0.1f, 0.1f, 0.9f * alpha);
                text_color = ImVec4(1.0f, 0.6f, 0.6f, alpha);
                break;
            case NotificationType::Info:
            default:
                bg_color = ImVec4(0.15f, 0.15f, 0.2f, 0.9f * alpha);
                text_color = ImVec4(1.0f, 1.0f, 1.0f, alpha);
                break;
        }

        // Set up window position (top-right, stacked vertically)
        ImGui::SetNextWindowPos(
            ImVec2(work_pos.x + work_size.x - NOTIFICATION_WIDTH - NOTIFICATION_PADDING,
                   y_offset),
            ImGuiCond_Always
        );
        ImGui::SetNextWindowSize(ImVec2(NOTIFICATION_WIDTH, 0.0f));
        ImGui::SetNextWindowBgAlpha(bg_color.w);

        // Window flags for notification appearance
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoNav |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoInputs;

        // Create a unique window name for each notification
        char window_name[64];
        std::snprintf(window_name, sizeof(window_name), "##Notification%zu", i);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));

        if (ImGui::Begin(window_name, nullptr, flags)) {
            ImGui::TextWrapped("%s", notif.message.c_str());

            // Update y_offset for next notification
            y_offset += ImGui::GetWindowHeight() + 5.0f;
        }
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}

void NotificationManager::clear() {
    m_notifications.clear();
}

void NotificationManager::cleanup_expired() {
    auto now = std::chrono::steady_clock::now();

    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
            [now](const Notification& notif) {
                float elapsed = std::chrono::duration<float>(now - notif.created_at).count();
                return elapsed >= notif.duration_seconds;
            }),
        m_notifications.end()
    );
}

} // namespace emu
