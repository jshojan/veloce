#pragma once

#include "game_plugin_common/timer_core.hpp"

namespace game_plugin_common {

// TimerPanel - Complete speedrun timer panel with timer display, splits, and controls
// This renders a full ImGui window with all standard speedrun timer components.
class TimerPanel {
public:
    TimerPanel();
    ~TimerPanel() = default;

    // Render the complete timer panel
    // visible: in/out parameter for window visibility
    // timer: the timer core to display and control
    void render(bool& visible, TimerCore& timer);

    // Get the panel name for menus
    const char* get_name() const { return "Speedrun Timer"; }

    // Display options
    bool show_timer = true;
    bool show_splits = true;
    bool show_delta = true;
    bool show_controls = true;

private:
    void render_timer_section(TimerCore& timer);
    void render_splits_section(TimerCore& timer);
    void render_controls_section(TimerCore& timer);
};

} // namespace game_plugin_common
