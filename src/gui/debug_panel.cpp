#include "debug_panel.hpp"
#include "../core/application.hpp"
#include "../core/plugin_manager.hpp"
#include "emu/emulator_plugin.hpp"

#include <imgui.h>
#include <cstdio>

namespace emu {

DebugPanel::DebugPanel() {
    // Add some default watches for NES
    m_watches.push_back({0x0000, "Zero Page 0", true});
    m_watches.push_back({0x00FF, "Stack Bottom", true});
    m_watches.push_back({0x0100, "Stack Top", true});
}

DebugPanel::~DebugPanel() = default;

void DebugPanel::render(Application& app, bool& visible) {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Debug Panel", &visible, ImGuiWindowFlags_MenuBar)) {
        auto* plugin = app.get_plugin_manager().get_active_plugin();

        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show ASCII", nullptr, &m_show_ascii);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Tabs for different debug views
        if (ImGui::BeginTabBar("DebugTabs")) {
            if (ImGui::BeginTabItem("Timing")) {
                render_timing_info(plugin);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Memory")) {
                render_memory_viewer(plugin);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("CPU")) {
                render_cpu_state(plugin);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("PPU")) {
                render_ppu_state(plugin);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void DebugPanel::render_timing_info(IEmulatorPlugin* plugin) {
    if (!plugin || !plugin->is_rom_loaded()) {
        ImGui::Text("No ROM loaded");
        return;
    }

    auto info = plugin->get_info();

    ImGui::Text("System: %s", info.name);
    ImGui::Text("Native FPS: %.4f", info.native_fps);
    if (info.cycles_per_second)
        ImGui::Text("CPU Clock: %lu Hz", static_cast<unsigned long>(info.cycles_per_second));
    else
        ImGui::TextUnformatted("CPU Clock: not measured");
    ImGui::Separator();

    ImGui::Text("Frame Count: %lu", static_cast<unsigned long>(plugin->get_frame_count()));
    ImGui::Text("Cycle Count: %lu", static_cast<unsigned long>(plugin->get_cycle_count()));

    if (info.cycles_per_second) {
        // Calculate emulated time
        double seconds = static_cast<double>(plugin->get_cycle_count()) / info.cycles_per_second;
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds - hours * 3600) / 60);
        int secs = static_cast<int>(seconds) % 60;
        int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);

        ImGui::Text("Emulated Time: %02d:%02d:%02d.%03d", hours, minutes, secs, ms);
    } else {
        ImGui::TextUnformatted("Emulated Time: not measured");
    }
}

void DebugPanel::render_memory_viewer(IEmulatorPlugin* plugin) {
    if (!plugin || !plugin->is_rom_loaded()) {
        ImGui::Text("No ROM loaded");
        return;
    }

    // Address input
    ImGui::SetNextItemWidth(100);
    ImGui::InputScalar("Start Address", ImGuiDataType_U16, &m_memory_start_address, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();

    // Quick navigation
    if (ImGui::Button("Zero Page")) m_memory_start_address = 0x0000;
    ImGui::SameLine();
    if (ImGui::Button("Stack")) m_memory_start_address = 0x0100;
    ImGui::SameLine();
    if (ImGui::Button("RAM")) m_memory_start_address = 0x0200;
    ImGui::SameLine();
    if (ImGui::Button("PPU")) m_memory_start_address = 0x2000;

    ImGui::Separator();

    // Memory display
    const int rows = 16;

    // Header
    ImGui::Text("       ");
    for (int i = 0; i < m_memory_columns; i++) {
        ImGui::SameLine();
        ImGui::Text("%02X", i);
    }
    if (m_show_ascii) {
        ImGui::SameLine();
        ImGui::Text(" ASCII");
    }

    // Memory rows
    for (int row = 0; row < rows; row++) {
        uint16_t addr = m_memory_start_address + row * m_memory_columns;
        ImGui::Text("%04X: ", addr);

        char ascii[17] = {0};

        for (int col = 0; col < m_memory_columns; col++) {
            uint16_t current_addr = addr + col;
            uint8_t value = plugin->read_memory(current_addr);

            ImGui::SameLine();
            ImGui::Text("%02X", value);

            // Build ASCII representation
            if (m_show_ascii) {
                ascii[col] = (value >= 32 && value < 127) ? static_cast<char>(value) : '.';
            }
        }

        if (m_show_ascii) {
            ImGui::SameLine();
            ImGui::Text(" %s", ascii);
        }
    }

    // Watch list
    ImGui::Separator();
    ImGui::Text("Watches:");

    for (size_t i = 0; i < m_watches.size(); i++) {
        auto& watch = m_watches[i];
        uint8_t value = plugin->read_memory(watch.address);

        if (watch.show_hex) {
            ImGui::Text("$%04X %-16s = $%02X (%3d)",
                watch.address, watch.label.c_str(), value, value);
        } else {
            ImGui::Text("$%04X %-16s = %3d",
                watch.address, watch.label.c_str(), value);
        }
    }
}

void DebugPanel::render_cpu_state(IEmulatorPlugin* plugin) {
    if (!plugin || !plugin->is_rom_loaded()) {
        ImGui::Text("No ROM loaded");
        return;
    }

    ImGui::Text("CPU State (NES 6502)");
    ImGui::Separator();

    // Read zero page memory for common CPU state display
    // These are common memory locations for NES games

    // Display common zero-page locations
    ImGui::Text("Zero Page Memory:");
    ImGui::Columns(4, nullptr, false);
    for (int i = 0; i < 16; i++) {
        ImGui::Text("$%02X: %02X", i, plugin->read_memory(i));
        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Text("Stack Page ($0100-$01FF):");
    ImGui::Columns(4, nullptr, false);
    for (int i = 0; i < 16; i++) {
        uint16_t addr = 0x01FF - i;
        ImGui::Text("$%04X: %02X", addr, plugin->read_memory(addr));
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

void DebugPanel::render_ppu_state(IEmulatorPlugin* plugin) {
    if (!plugin || !plugin->is_rom_loaded()) {
        ImGui::Text("No ROM loaded");
        return;
    }

    ImGui::Text("PPU State (NES 2C02)");
    ImGui::Separator();

    // PPU registers are memory-mapped at $2000-$2007
    ImGui::Text("PPU Registers ($2000-$2007):");

    uint8_t ppuctrl = plugin->read_memory(0x2000);
    uint8_t ppumask = plugin->read_memory(0x2001);
    uint8_t ppustatus = plugin->read_memory(0x2002);

    ImGui::Text("PPUCTRL  ($2000): $%02X", ppuctrl);
    ImGui::Text("  - Base NT: %d", ppuctrl & 0x03);
    ImGui::Text("  - VRAM Inc: %s", (ppuctrl & 0x04) ? "32 (down)" : "1 (across)");
    ImGui::Text("  - Sprite PT: $%04X", (ppuctrl & 0x08) ? 0x1000 : 0x0000);
    ImGui::Text("  - BG PT: $%04X", (ppuctrl & 0x10) ? 0x1000 : 0x0000);
    ImGui::Text("  - Sprite Size: %s", (ppuctrl & 0x20) ? "8x16" : "8x8");
    ImGui::Text("  - NMI Enable: %s", (ppuctrl & 0x80) ? "Yes" : "No");

    ImGui::Separator();

    ImGui::Text("PPUMASK  ($2001): $%02X", ppumask);
    ImGui::Text("  - Grayscale: %s", (ppumask & 0x01) ? "Yes" : "No");
    ImGui::Text("  - Show BG left: %s", (ppumask & 0x02) ? "Yes" : "No");
    ImGui::Text("  - Show Spr left: %s", (ppumask & 0x04) ? "Yes" : "No");
    ImGui::Text("  - Show BG: %s", (ppumask & 0x08) ? "Yes" : "No");
    ImGui::Text("  - Show Sprites: %s", (ppumask & 0x10) ? "Yes" : "No");

    ImGui::Separator();

    ImGui::Text("PPUSTATUS ($2002): $%02X", ppustatus);
    ImGui::Text("  - Sprite Overflow: %s", (ppustatus & 0x20) ? "Yes" : "No");
    ImGui::Text("  - Sprite 0 Hit: %s", (ppustatus & 0x40) ? "Yes" : "No");
    ImGui::Text("  - VBlank: %s", (ppustatus & 0x80) ? "Yes" : "No");
}

} // namespace emu
