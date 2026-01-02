#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace emu {

// Type of notification - affects color/style
enum class NotificationType {
    Info,       // General information (white/gray)
    Success,    // Successful operation (green)
    Warning,    // Warning message (yellow)
    Error       // Error/failure (red)
};

// A single notification message
struct Notification {
    std::string message;
    NotificationType type;
    std::chrono::steady_clock::time_point created_at;
    float duration_seconds;  // How long to show the notification
};

// Manages on-screen notifications (toast messages)
// These are temporary messages shown to provide feedback for user actions
class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    // Add a notification to be displayed
    // duration_seconds: How long to display (default 2.0 seconds)
    void add(const std::string& message,
             NotificationType type = NotificationType::Info,
             float duration_seconds = 2.0f);

    // Convenience methods for common notification types
    void info(const std::string& message, float duration = 2.0f);
    void success(const std::string& message, float duration = 2.0f);
    void warning(const std::string& message, float duration = 2.0f);
    void error(const std::string& message, float duration = 2.0f);

    // Render all active notifications
    // Should be called during GUI rendering phase
    void render();

    // Clear all notifications
    void clear();

    // Get number of active notifications
    size_t count() const { return m_notifications.size(); }

private:
    // Remove expired notifications
    void cleanup_expired();

    std::vector<Notification> m_notifications;

    // Configuration
    static constexpr float NOTIFICATION_PADDING = 10.0f;
    static constexpr float NOTIFICATION_WIDTH = 300.0f;
    static constexpr float NOTIFICATION_FADE_TIME = 0.3f;  // Fade out duration
    static constexpr size_t MAX_NOTIFICATIONS = 5;  // Maximum visible at once
};

} // namespace emu
