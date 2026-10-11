#pragma once

#include "emu/input_types.hpp"
#include "emu/controller_layout.hpp"
#include <string>

struct ImVec2;

namespace emu {

class Application;

class InputConfigPanel {
public:
    InputConfigPanel();
    ~InputConfigPanel();

    // Render the input configuration panel
    // Returns true if the settings window should close (e.g., after saving)
    bool render(Application& app);

    // Check if currently waiting for input
    bool is_capturing_input() const { return m_waiting_for_input != VirtualButton::COUNT; }

    // Cancel input capture
    void cancel_capture();

private:
    void render_platform_selector(Application& app);
    void render_controller_selector(Application& app);
    void render_controller_visual(Application& app);
    void render_binding_table(Application& app);
    void render_binding_row(Application& app, VirtualButton button);
    void check_for_input(Application& app);

    // Draw controller shape and buttons
    void draw_controller_body(ImVec2 origin, ImVec2 size, ControllerShape shape);
    void draw_button(Application& app, ImVec2 origin, ImVec2 size, const ButtonLayout& button);

    // State
    int m_selected_port = 0;
    int m_selected_controller = -1;  // -1 for keyboard
    VirtualButton m_waiting_for_input = VirtualButton::COUNT;
    VirtualButton m_hovered_button = VirtualButton::COUNT;
    std::string m_current_platform = "NES";

    // Visual mode toggle
    bool m_show_visual = true;
};

} // namespace emu
