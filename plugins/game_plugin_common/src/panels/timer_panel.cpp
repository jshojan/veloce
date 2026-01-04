#include "game_plugin_common/panels/timer_panel.hpp"
#include "game_plugin_common/timer_core.hpp"
#include <imgui.h>

namespace game_plugin_common {

TimerPanel::TimerPanel() = default;

void TimerPanel::render(bool& visible, TimerCore& timer) {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Speedrun Timer", &visible)) {
        if (show_timer) {
            render_timer_section(timer);
            ImGui::Separator();
        }
        if (show_splits) {
            render_splits_section(timer);
            ImGui::Separator();
        }
        if (show_controls) {
            render_controls_section(timer);
        }
    }
    ImGui::End();
}

void TimerPanel::render_timer_section(TimerCore& timer) {
    // Current time display
    uint64_t current_time = timer.get_current_time_ms();
    std::string time_str = format_time(current_time);

    // Color based on timer state
    ImVec4 timer_color;
    switch (timer.get_state()) {
        case emu::TimerState::Running:
            timer_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);   // Green when running
            break;
        case emu::TimerState::Paused:
            timer_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);   // Yellow when paused
            break;
        case emu::TimerState::Finished:
            timer_color = ImVec4(0.2f, 0.6f, 0.8f, 1.0f);   // Blue when finished
            break;
        default:
            timer_color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);   // Grey when stopped
            break;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, timer_color);

    // Center the timer
    float text_width = ImGui::CalcTextSize(time_str.c_str()).x;
    float window_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((window_width - text_width) * 0.5f);

    ImGui::TextUnformatted(time_str.c_str());
    ImGui::PopStyleColor();

    // Game and category info
    const auto& game_name = timer.get_game_name();
    if (!game_name.empty()) {
        ImGui::TextDisabled("%s - %s", game_name.c_str(), timer.get_category().c_str());
    }

    // Attempt count
    int attempt_count = timer.get_attempt_count();
    if (attempt_count > 0) {
        ImGui::TextDisabled("Attempts: %d/%d", timer.get_completed_count(), attempt_count);
    }

    // Sum of Best
    uint64_t sob = timer.get_sum_of_best_ms();
    if (sob > 0) {
        ImGui::Text("SoB: %s", format_time(sob).c_str());

        // Best possible time
        uint64_t bpt = timer.get_best_possible_time_ms();
        if (bpt > 0 && timer.get_state() == emu::TimerState::Running) {
            ImGui::SameLine();
            ImGui::TextDisabled("BPT: %s", format_time(bpt).c_str());
        }
    }
}

void TimerPanel::render_splits_section(TimerCore& timer) {
    int total_splits = timer.get_total_splits();
    int current_split = timer.get_current_split_index();

    if (total_splits == 0) {
        ImGui::TextDisabled("No splits loaded");
        ImGui::TextDisabled("Load a game with a speedrun plugin");
        return;
    }

    // Splits table
    if (ImGui::BeginTable("Splits", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Split", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (int i = 0; i < total_splits; i++) {
            // Get split timing
            emu::SplitTiming timing = timer.get_split_timing(i);
            const char* split_name = timer.get_split_name(i);

            ImGui::TableNextRow();

            // Highlight current split
            if (i == current_split) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.5f, 0.5f)));
            }

            // Split name
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(split_name ? split_name : "");

            // Delta vs comparison
            ImGui::TableNextColumn();
            if (timing.time_ms > 0 && show_delta) {
                int64_t delta = timing.delta_ms;

                // Color based on delta and special states
                ImVec4 delta_color;
                if (timing.is_gold) {
                    delta_color = ImVec4(0.8f, 0.7f, 0.2f, 1.0f);   // Gold for best segment
                } else if (delta <= 0) {
                    delta_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);   // Green (ahead)
                } else {
                    delta_color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);   // Red (behind)
                }

                ImGui::PushStyleColor(ImGuiCol_Text, delta_color);
                ImGui::TextUnformatted(format_delta(delta).c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("-");
            }

            // Split time
            ImGui::TableNextColumn();
            if (timing.time_ms > 0) {
                ImGui::TextUnformatted(format_time(timing.time_ms, false).c_str());
            } else {
                ImGui::TextDisabled("-");
            }
        }

        ImGui::EndTable();
    }
}

void TimerPanel::render_controls_section(TimerCore& timer) {
    float button_width = 60;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 4 + spacing * 3;
    float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

    if (timer.get_state() == emu::TimerState::Running) {
        if (ImGui::Button("Split", ImVec2(button_width, 0))) {
            timer.split();
        }
    } else {
        if (ImGui::Button("Start", ImVec2(button_width, 0))) {
            timer.start();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Undo", ImVec2(button_width, 0))) {
        timer.undo_split();
    }

    ImGui::SameLine();
    if (ImGui::Button("Skip", ImVec2(button_width, 0))) {
        timer.skip_split();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(button_width, 0))) {
        timer.reset();
    }

    // Comparison type selector
    ImGui::Spacing();
    int comparison = static_cast<int>(timer.get_comparison_type());
    const char* comparison_items[] = { "Personal Best", "Best Segments" };
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("Compare", &comparison, comparison_items, 2)) {
        timer.set_comparison_type(static_cast<emu::ComparisonType>(comparison));
    }

    // Keyboard shortcuts hint
    ImGui::TextDisabled("Numpad1=Split, Numpad3=Reset");
}

} // namespace game_plugin_common
