#include "input_config_panel.hpp"
#include "core/application.hpp"
#include "core/input_manager.hpp"
#include "core/plugin_manager.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <SDL.h>
#include <algorithm>
#include <vector>

namespace emu {

// Helper to get available platforms from loaded plugins
static std::vector<std::string> get_available_platforms(const PluginManager& plugins) {
    std::vector<std::string> platforms;
    for (const auto& plugin : plugins.get_plugins()) {
        if (plugin.instance) {
            platforms.push_back(plugin.name);
        }
    }
    return platforms;
}

// Helper to get controller layout for a platform from the plugin
static const ControllerLayoutInfo* get_plugin_layout(const PluginManager& plugins, const std::string& platform) {
    for (const auto& plugin : plugins.get_plugins()) {
        if (plugin.instance && plugin.name == platform) {
            return plugin.instance->get_controller_layout();
        }
    }
    return nullptr;
}

// Colors for controller rendering
namespace colors {
    const ImU32 CONTROLLER_BODY = IM_COL32(60, 60, 70, 255);
    const ImU32 CONTROLLER_OUTLINE = IM_COL32(100, 100, 110, 255);
    const ImU32 BUTTON_NORMAL = IM_COL32(80, 80, 90, 255);
    const ImU32 BUTTON_HOVER = IM_COL32(100, 120, 180, 255);
    const ImU32 BUTTON_WAITING = IM_COL32(180, 80, 80, 255);
    const ImU32 BUTTON_BOUND = IM_COL32(70, 130, 70, 255);
    const ImU32 BUTTON_OUTLINE = IM_COL32(150, 150, 160, 255);
    const ImU32 DPAD_BG = IM_COL32(50, 50, 60, 255);
    const ImU32 TEXT_NORMAL = IM_COL32(220, 220, 220, 255);
    const ImU32 TEXT_DIM = IM_COL32(150, 150, 150, 255);
}

InputConfigPanel::InputConfigPanel() = default;
InputConfigPanel::~InputConfigPanel() = default;

bool InputConfigPanel::render(Application& app) {
    bool should_close = false;
    auto& input = app.get_input_manager();

    // Set input capture mode while the input config panel is visible
    // This blocks game input so the game doesn't receive inputs during configuration
    input.set_input_capture_mode(true);

    // Get current platform from input manager
    m_current_platform = input.get_current_platform();

    // Sync selected controller with input manager
    m_selected_controller = input.get_active_controller();

    // Platform selector dropdown
    render_platform_selector(app);

    // Toggle between visual and table view
    ImGui::SameLine(ImGui::GetWindowWidth() - 150);
    ImGui::Checkbox("Visual Mode", &m_show_visual);

    ImGui::Separator();

    // Controller selector
    render_controller_selector(app);
    if (m_current_platform=="N64") {
        ImGui::TextWrapped("Stick: selected gamepad left stick, or T/F/G/H for up/left/down/right on keyboard. Arrow bindings control the D-pad.");
        ImGui::TextWrapped("Stick defaults: 15%% dead zone, range +/-80. Z: left trigger; C buttons: right stick. Extra players use these defaults; player one custom bindings override its button defaults.");
    }

    ImGui::Separator();

    // Visual controller or binding table
    if (m_show_visual) {
        render_controller_visual(app);
    } else {
        render_binding_table(app);
    }

    ImGui::Separator();

    // Action buttons
    if (ImGui::Button("Reset to Defaults")) {
        input.load_default_bindings_for_platform(m_current_platform);
    }

    ImGui::SameLine();

    if (ImGui::Button("Close")) {
        should_close = true;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(Changes are saved automatically)");

    // Show help text when capturing
    if (m_waiting_for_input != VirtualButton::COUNT) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("Press a key or gamepad button to bind '%s'...",
            InputManager::get_button_name(m_waiting_for_input).c_str());
        ImGui::Text("Press Escape to cancel");
        ImGui::PopStyleColor();

        check_for_input(app);
    }

    // Show tooltip for hovered button
    if (m_hovered_button != VirtualButton::COUNT && m_waiting_for_input == VirtualButton::COUNT) {
        const InputBinding* binding = input.get_binding(m_hovered_button);
        std::string btn_name = InputManager::get_button_name(m_hovered_button);

        ImGui::BeginTooltip();
        ImGui::Text("%s", btn_name.c_str());
        if (binding) {
            ImGui::Text("Bound to: %s", InputManager::get_binding_display_name(*binding).c_str());
        } else {
            ImGui::TextDisabled("Not bound - click to assign");
        }
        ImGui::EndTooltip();
    }

    m_hovered_button = VirtualButton::COUNT;  // Reset for next frame

    // Clear capture mode when panel is closing and auto-save
    if (should_close) {
        input.set_input_capture_mode(false);
        m_waiting_for_input = VirtualButton::COUNT;  // Cancel any pending capture
        input.save_platform_config(m_current_platform);  // Auto-save on close
    }

    return should_close;
}

void InputConfigPanel::render_controller_selector(Application& app) {
    auto& input = app.get_input_manager();
    auto controllers = input.get_available_controllers();

    // Find current selection index
    int current_index = 0;
    for (size_t i = 0; i < controllers.size(); i++) {
        if (controllers[i].first == m_selected_controller) {
            current_index = static_cast<int>(i);
            break;
        }
    }

    // Build combo items
    std::string preview = controllers.empty() ? "No controllers" : controllers[current_index].second;

    ImGui::Text("Controller:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);

    if (ImGui::BeginCombo("##Controller", preview.c_str())) {
        for (size_t i = 0; i < controllers.size(); i++) {
            bool is_selected = (current_index == static_cast<int>(i));
            ImGui::PushID(static_cast<int>(i));
            const auto label=controllers[i].second+(controllers[i].first<0 ? std::string{} : " (#"+std::to_string(controllers[i].first+1)+")");
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                m_selected_controller = controllers[i].first;
                input.set_active_controller(m_selected_controller);
                // Save config immediately so controller choice persists
                input.save_platform_config(m_current_platform);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    const auto* layout=input.get_controller_layout();
    const int ports=layout ? std::clamp(layout->controller_count,1,4) : 1;
    for (int port=1;port<ports;++port) {
        ImGui::PushID(port);
        ImGui::Text("Player %d:",port+1);ImGui::SameLine();ImGui::SetNextItemWidth(200);
        const int selected=input.get_controller_for_port(port);
        const auto preview=selected<0 ? std::string("Unassigned") : input.get_controller_name(selected)+" (#"+std::to_string(selected+1)+")";
        if (ImGui::BeginCombo("##Port",preview.c_str())) {
            if (ImGui::Selectable("Unassigned",selected<0)) {input.set_controller_for_port(port,-1);input.save_platform_config(m_current_platform);}
            for (int i=0;i<input.get_connected_controller_count();++i) {
                const auto label=input.get_controller_name(i)+" (#"+std::to_string(i+1)+")";
                if (ImGui::Selectable(label.c_str(),selected==i)) {input.set_controller_for_port(port,i);input.save_platform_config(m_current_platform);}
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
    if (ports>1) ImGui::TextWrapped("Extra players use standard gamepad buttons. Assigning a pad moves it from its previous port. Identical pads without serial numbers may need reselection after reconnecting.");

    // Show controller status
    int controller_count = input.get_connected_controller_count();
    if (controller_count > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d gamepad%s connected)", controller_count,
            controller_count > 1 ? "s" : "");
    }
}

void InputConfigPanel::render_platform_selector(Application& app) {
    auto& input = app.get_input_manager();
    auto& plugins = app.get_plugin_manager();

    // Get available platforms from loaded plugins
    auto available_platforms = get_available_platforms(plugins);

    // If no plugins loaded, show message
    if (available_platforms.empty()) {
        ImGui::TextDisabled("No emulator plugins loaded");
        return;
    }

    // Find current platform index
    int current_index = 0;
    for (size_t i = 0; i < available_platforms.size(); i++) {
        if (available_platforms[i] == m_current_platform) {
            current_index = static_cast<int>(i);
            break;
        }
    }

    // If current platform is not in list, switch to first available
    if (std::find(available_platforms.begin(), available_platforms.end(), m_current_platform) == available_platforms.end()) {
        m_current_platform = available_platforms[0];
        input.set_current_platform(m_current_platform);
        current_index = 0;
    }

    ImGui::Text("Platform:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);

    if (ImGui::BeginCombo("##Platform", m_current_platform.c_str())) {
        for (size_t i = 0; i < available_platforms.size(); i++) {
            bool is_selected = (current_index == static_cast<int>(i));
            if (ImGui::Selectable(available_platforms[i].c_str(), is_selected)) {
                // Save current platform config before switching
                input.save_platform_config(m_current_platform);
                // Switch to new platform
                input.set_current_platform(available_platforms[i]);
                m_current_platform = available_platforms[i];
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void InputConfigPanel::render_controller_visual(Application& app) {
    auto& input = app.get_input_manager();
    auto& plugins = app.get_plugin_manager();

    // Get controller layout from the plugin for this platform
    const ControllerLayoutInfo* layout = get_plugin_layout(plugins, m_current_platform);
    if (!layout) {
        ImGui::TextDisabled("No controller layout available for %s", m_current_platform.c_str());
        return;
    }

    // Calculate controller display size
    ImVec2 available = ImGui::GetContentRegionAvail();
    float max_width = available.x - 20;
    float max_height = available.y - 60;  // Leave room for buttons

    // Maintain aspect ratio
    float controller_width, controller_height;
    if (layout->aspect_ratio >= 1.0f) {
        // Wider than tall
        controller_width = std::min(max_width, 400.0f);
        controller_height = controller_width / layout->aspect_ratio;
        if (controller_height > max_height) {
            controller_height = max_height;
            controller_width = controller_height * layout->aspect_ratio;
        }
    } else {
        // Taller than wide
        controller_height = std::min(max_height, 300.0f);
        controller_width = controller_height * layout->aspect_ratio;
        if (controller_width > max_width) {
            controller_width = max_width;
            controller_height = controller_width / layout->aspect_ratio;
        }
    }

    // Center the controller
    float offset_x = (available.x - controller_width) * 0.5f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 origin(cursor.x + offset_x, cursor.y + 10);
    ImVec2 size(controller_width, controller_height);

    // Reserve space
    ImGui::Dummy(ImVec2(available.x, controller_height + 20));

    // Draw controller body
    draw_controller_body(origin, size, layout->shape);

    // Draw each button
    for (int i = 0; i < layout->button_count; i++) {
        draw_button(app, origin, size, layout->buttons[i]);
    }
}

void InputConfigPanel::draw_controller_body(ImVec2 origin, ImVec2 size, ControllerShape shape) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float x = origin.x;
    float y = origin.y;
    float w = size.x;
    float h = size.y;
    float r = 15.0f;  // Corner radius

    switch (shape) {
        case ControllerShape::Rectangle:
            // NES-style rectangular controller
            draw_list->AddRectFilled(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_BODY, r);
            draw_list->AddRect(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_OUTLINE, r, 0, 2.0f);
            break;

        case ControllerShape::DogBone:
            // SNES-style with curved ends
            {
                float indent = w * 0.15f;
                draw_list->AddRectFilled(
                    ImVec2(x + indent, y), ImVec2(x + w - indent, y + h),
                    colors::CONTROLLER_BODY, r);
                // Left grip
                draw_list->AddCircleFilled(
                    ImVec2(x + indent, y + h/2), h * 0.45f,
                    colors::CONTROLLER_BODY);
                // Right grip
                draw_list->AddCircleFilled(
                    ImVec2(x + w - indent, y + h/2), h * 0.45f,
                    colors::CONTROLLER_BODY);
            }
            break;

        case ControllerShape::Handheld:
            // Game Boy style
            draw_list->AddRectFilled(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_BODY, r * 0.5f);
            draw_list->AddRect(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_OUTLINE, r * 0.5f, 0, 2.0f);
            // Screen area (darker)
            draw_list->AddRectFilled(
                ImVec2(x + w*0.15f, y + h*0.1f),
                ImVec2(x + w*0.85f, y + h*0.35f),
                IM_COL32(30, 40, 30, 255), 3.0f);
            break;

        case ControllerShape::Modern:
        default:
            // Modern controller with grips
            draw_list->AddRectFilled(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_BODY, r);
            draw_list->AddRect(
                ImVec2(x, y), ImVec2(x + w, y + h),
                colors::CONTROLLER_OUTLINE, r, 0, 2.0f);
            break;
    }
}

void InputConfigPanel::draw_button(Application& app, ImVec2 origin, ImVec2 size,
                                    const ButtonLayout& button) {
    auto& input = app.get_input_manager();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Calculate button position
    float btn_x = origin.x + button.x * size.x - (button.width * size.x) / 2;
    float btn_y = origin.y + button.y * size.y - (button.height * size.y) / 2;
    float btn_w = button.width * size.x;
    float btn_h = button.height * size.y;

    ImVec2 btn_min(btn_x, btn_y);
    ImVec2 btn_max(btn_x + btn_w, btn_y + btn_h);

    // Check if mouse is hovering
    ImVec2 mouse = ImGui::GetMousePos();
    bool is_hovered = (mouse.x >= btn_min.x && mouse.x <= btn_max.x &&
                       mouse.y >= btn_min.y && mouse.y <= btn_max.y);

    bool is_waiting = (m_waiting_for_input == button.button);
    bool is_bound = (input.get_binding(button.button) != nullptr);

    // Determine button color
    ImU32 btn_color;
    if (is_waiting) {
        btn_color = colors::BUTTON_WAITING;
    } else if (is_hovered) {
        btn_color = colors::BUTTON_HOVER;
        m_hovered_button = button.button;
    } else if (is_bound) {
        btn_color = colors::BUTTON_BOUND;
    } else {
        btn_color = colors::BUTTON_NORMAL;
    }

    // Draw button
    if (button.is_dpad) {
        // D-pad buttons are rectangular
        draw_list->AddRectFilled(btn_min, btn_max, btn_color, 2.0f);
        draw_list->AddRect(btn_min, btn_max, colors::BUTTON_OUTLINE, 2.0f, 0, 1.0f);
    } else if (button.button == VirtualButton::Start || button.button == VirtualButton::Select) {
        // Start/Select are pill-shaped
        float radius = btn_h / 2;
        draw_list->AddRectFilled(btn_min, btn_max, btn_color, radius);
        draw_list->AddRect(btn_min, btn_max, colors::BUTTON_OUTLINE, radius, 0, 1.0f);
    } else {
        // Face buttons are circular
        float radius = std::min(btn_w, btn_h) / 2;
        ImVec2 center((btn_min.x + btn_max.x) / 2, (btn_min.y + btn_max.y) / 2);
        draw_list->AddCircleFilled(center, radius, btn_color);
        draw_list->AddCircle(center, radius, colors::BUTTON_OUTLINE, 0, 1.5f);

        // Draw button label inside
        ImVec2 text_size = ImGui::CalcTextSize(button.label);
        ImVec2 text_pos(center.x - text_size.x/2, center.y - text_size.y/2);
        draw_list->AddText(text_pos, colors::TEXT_NORMAL, button.label);
    }

    // Draw label for D-pad and Start/Select (outside the button)
    if (button.is_dpad || button.button == VirtualButton::Start || button.button == VirtualButton::Select) {
        if (!button.is_dpad) {
            // Start/Select label inside
            ImVec2 text_size = ImGui::CalcTextSize(button.label);
            ImVec2 text_pos((btn_min.x + btn_max.x)/2 - text_size.x/2,
                           (btn_min.y + btn_max.y)/2 - text_size.y/2);
            draw_list->AddText(text_pos, colors::TEXT_NORMAL, button.label);
        }
    }

    // Handle click
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_waiting_for_input == VirtualButton::COUNT) {
            m_waiting_for_input = button.button;
        }
    }

    // Right-click to clear binding
    if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        input.clear_binding(button.button);
    }
}

void InputConfigPanel::render_binding_table(Application& app) {
    auto buttons = InputManager::get_platform_buttons(m_current_platform);

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("Bindings", 3, flags)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        for (auto button : buttons) {
            render_binding_row(app, button);
        }

        ImGui::EndTable();
    }
}

void InputConfigPanel::render_binding_row(Application& app, VirtualButton button) {
    auto& input = app.get_input_manager();

    ImGui::TableNextRow();

    // Button name column
    ImGui::TableNextColumn();
    std::string button_name = InputManager::get_button_name(button);
    ImGui::Text("%s", button_name.c_str());

    // Binding column
    ImGui::TableNextColumn();

    bool is_waiting = (m_waiting_for_input == button);
    if (is_waiting) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("[ Press a key... ]");
        ImGui::PopStyleColor();
    } else {
        const InputBinding* binding = input.get_binding(button);
        if (binding) {
            std::string display = InputManager::get_binding_display_name(*binding);
            ImGui::Text("%s", display.c_str());
        } else {
            ImGui::TextDisabled("(not bound)");
        }
    }

    // Action column
    ImGui::TableNextColumn();

    // Unique IDs for buttons
    std::string set_id = "Set##" + button_name;
    std::string clear_id = "Clear##" + button_name;

    if (is_waiting) {
        if (ImGui::Button(("Cancel##" + button_name).c_str())) {
            m_waiting_for_input = VirtualButton::COUNT;
        }
    } else {
        if (ImGui::Button(set_id.c_str())) {
            m_waiting_for_input = button;
        }

        ImGui::SameLine();

        if (ImGui::Button(clear_id.c_str())) {
            input.clear_binding(button);
        }
    }
}

void InputConfigPanel::check_for_input(Application& app) {
    auto& input = app.get_input_manager();

    // Check for Escape to cancel
    if (input.is_key_pressed(SDL_SCANCODE_ESCAPE)) {
        m_waiting_for_input = VirtualButton::COUNT;
        return;
    }

    // Check keyboard keys (skip Escape which is cancel)
    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        if (i == SDL_SCANCODE_ESCAPE) continue;

        if (keyboard[i]) {
            InputBinding binding;
            binding.type = InputSourceType::Keyboard;
            binding.device_id = -1;
            binding.code = i;
            binding.axis_threshold = 0;
            binding.axis_positive = false;

            input.set_binding(m_waiting_for_input, binding);
            m_waiting_for_input = VirtualButton::COUNT;
            return;
        }
    }

    // Check gamepad buttons and axes
    int controller_count = input.get_connected_controller_count();
    for (int c = 0; c < controller_count; c++) {
        // Check buttons
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
            if (input.is_gamepad_button_pressed(c, b)) {
                InputBinding binding;
                binding.type = InputSourceType::GamepadButton;
                binding.device_id = input.get_controller_instance_id(c);
                binding.code = b;
                binding.axis_threshold = 0;
                binding.axis_positive = false;

                input.set_binding(m_waiting_for_input, binding);
                m_waiting_for_input = VirtualButton::COUNT;
                return;
            }
        }

        // Check axes
        for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
            float value = input.get_gamepad_axis(c, a);
            if (value > 0.5f || value < -0.5f) {
                InputBinding binding;
                binding.type = InputSourceType::GamepadAxis;
                binding.device_id = input.get_controller_instance_id(c);
                binding.code = a;
                binding.axis_threshold = 0.5f;
                binding.axis_positive = (value > 0);

                input.set_binding(m_waiting_for_input, binding);
                m_waiting_for_input = VirtualButton::COUNT;
                return;
            }
        }
    }
}

void InputConfigPanel::cancel_capture() {
    m_waiting_for_input = VirtualButton::COUNT;
}

} // namespace emu
