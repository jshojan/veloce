#include "emu/tas_plugin.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <deque>
#include <map>

namespace {

// Simple FM2 format support (NES movie format)
const char* s_file_formats[] = {".fm2", ".tas", nullptr};

class DefaultTASPlugin : public emu::ITASPlugin {
public:
    DefaultTASPlugin() = default;
    ~DefaultTASPlugin() override = default;

    emu::TASPluginInfo get_info() override {
        return {
            "TAS Editor",
            "1.0.0",
            "Veloce Team",
            "Tool-assisted speedrun movie editor and player. Features greenzone "
            "savestate snapshots, undo/redo support, frame editing, and FM2 import.",
            s_file_formats
        };
    }

    bool initialize(emu::ITASHost* host) override {
        m_host = host;
        return true;
    }

    void shutdown() override {
        close_movie();
        m_host = nullptr;
    }

    bool new_movie(const char* filename, bool from_savestate) override {
        close_movie();

        m_filename = filename;
        m_info = {};
        std::strncpy(m_info.platform, m_host->get_platform_name(), sizeof(m_info.platform) - 1);
        std::strncpy(m_info.rom_name, m_host->get_rom_name(), sizeof(m_info.rom_name) - 1);
        m_info.rom_crc32 = m_host->get_rom_crc32();
        m_info.starts_from_savestate = from_savestate;
        m_info.frame_count = 0;
        m_info.rerecord_count = 0;

        if (from_savestate) {
            m_host->save_state_to_buffer(m_start_state);
        }

        m_frames.clear();
        m_movie_loaded = true;
        m_mode = emu::TASMode::Recording;

        return true;
    }

    bool open_movie(const char* filename) override {
        close_movie();

        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        // Read header
        char magic[4];
        file.read(magic, 4);
        if (std::strncmp(magic, "TAS1", 4) != 0) {
            return load_fm2(filename);  // Try FM2 format
        }

        file.read(reinterpret_cast<char*>(&m_info), sizeof(m_info));

        // Read start state if present
        if (m_info.starts_from_savestate) {
            uint32_t state_size;
            file.read(reinterpret_cast<char*>(&state_size), sizeof(state_size));
            m_start_state.resize(state_size);
            file.read(reinterpret_cast<char*>(m_start_state.data()), state_size);
        }

        // Read frames
        m_frames.resize(m_info.frame_count);
        for (uint64_t i = 0; i < m_info.frame_count; i++) {
            file.read(reinterpret_cast<char*>(&m_frames[i]), sizeof(emu::TASFrameData));
        }

        m_filename = filename;
        m_movie_loaded = true;
        m_mode = emu::TASMode::Stopped;

        return true;
    }

    bool save_movie() override {
        return save_movie_as(m_filename.c_str());
    }

    bool save_movie_as(const char* filename) override {
        if (!m_movie_loaded) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        // Update frame count
        m_info.frame_count = m_frames.size();

        // Write header
        file.write("TAS1", 4);
        file.write(reinterpret_cast<const char*>(&m_info), sizeof(m_info));

        // Write start state if present
        if (m_info.starts_from_savestate) {
            uint32_t state_size = static_cast<uint32_t>(m_start_state.size());
            file.write(reinterpret_cast<const char*>(&state_size), sizeof(state_size));
            file.write(reinterpret_cast<const char*>(m_start_state.data()), state_size);
        }

        // Write frames
        for (const auto& frame : m_frames) {
            file.write(reinterpret_cast<const char*>(&frame), sizeof(emu::TASFrameData));
        }

        m_filename = filename;
        return true;
    }

    void close_movie() override {
        m_frames.clear();
        m_start_state.clear();
        m_greenzone.clear();
        m_markers.clear();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_movie_loaded = false;
        m_mode = emu::TASMode::Stopped;
        m_current_frame = 0;
    }

    bool is_movie_loaded() const override {
        return m_movie_loaded;
    }

    emu::TASMovieInfo get_movie_info() const override {
        return m_info;
    }

    void set_movie_info(const emu::TASMovieInfo& info) override {
        m_info = info;
    }

    emu::TASMode get_mode() const override {
        return m_mode;
    }

    void set_mode(emu::TASMode mode) override {
        m_mode = mode;
    }

    void start_recording() override {
        m_mode = emu::TASMode::Recording;
        m_host->reset_emulator();
        m_current_frame = 0;
        m_frames.clear();
    }

    void stop_recording() override {
        m_mode = emu::TASMode::Stopped;
        m_info.frame_count = m_frames.size();
    }

    void start_playback() override {
        if (!m_movie_loaded || m_frames.empty()) return;

        m_mode = emu::TASMode::Playing;
        m_current_frame = 0;

        if (m_info.starts_from_savestate && !m_start_state.empty()) {
            m_host->load_state_from_buffer(m_start_state);
        } else {
            m_host->reset_emulator();
        }
    }

    void stop_playback() override {
        m_mode = emu::TASMode::Stopped;
    }

    uint32_t on_frame(int controller) override {
        if (!m_movie_loaded) return 0;

        uint32_t input = 0;

        switch (m_mode) {
            case emu::TASMode::Recording: {
                // Get input from host and record it
                input = m_host->get_controller_input(controller);

                if (controller == 0) {
                    emu::TASFrameData frame{};
                    frame.frame_number = m_current_frame;
                    for (int i = 0; i < 4; i++) {
                        frame.controller_inputs[i] = m_host->get_controller_input(i);
                    }
                    m_frames.push_back(frame);
                    m_current_frame++;

                    // Save greenzone periodically
                    if (m_current_frame % 60 == 0) {
                        std::vector<uint8_t> state;
                        if (m_host->save_state_to_buffer(state)) {
                            m_greenzone[m_current_frame] = std::move(state);
                        }
                    }
                }
                break;
            }

            case emu::TASMode::Playing:
            case emu::TASMode::ReadOnly: {
                if (m_current_frame < m_frames.size()) {
                    input = m_frames[m_current_frame].controller_inputs[controller];
                    if (controller == 0) {
                        m_current_frame++;
                    }
                } else {
                    // End of movie
                    m_mode = emu::TASMode::Stopped;
                }
                break;
            }

            case emu::TASMode::ReadWrite: {
                if (m_current_frame < m_frames.size()) {
                    input = m_frames[m_current_frame].controller_inputs[controller];
                    if (controller == 0) {
                        m_current_frame++;
                    }
                }
                break;
            }

            default:
                break;
        }

        return input;
    }

    emu::TASFrameData get_frame(uint64_t frame) const override {
        if (frame < m_frames.size()) {
            return m_frames[frame];
        }
        return {};
    }

    void set_frame(uint64_t frame, const emu::TASFrameData& data) override {
        if (frame < m_frames.size()) {
            save_undo_state();
            m_frames[frame] = data;
            invalidate_greenzone(frame);
        }
    }

    void insert_frame(uint64_t after_frame) override {
        save_undo_state();
        emu::TASFrameData new_frame{};
        new_frame.frame_number = after_frame + 1;
        if (after_frame < m_frames.size()) {
            m_frames.insert(m_frames.begin() + after_frame + 1, new_frame);
        } else {
            m_frames.push_back(new_frame);
        }
        // Renumber frames
        for (uint64_t i = after_frame + 1; i < m_frames.size(); i++) {
            m_frames[i].frame_number = i;
        }
        invalidate_greenzone(after_frame);
    }

    void delete_frame(uint64_t frame) override {
        if (frame < m_frames.size()) {
            save_undo_state();
            m_frames.erase(m_frames.begin() + frame);
            // Renumber frames
            for (uint64_t i = frame; i < m_frames.size(); i++) {
                m_frames[i].frame_number = i;
            }
            invalidate_greenzone(frame);
        }
    }

    void clear_input(uint64_t start_frame, uint64_t end_frame) override {
        save_undo_state();
        for (uint64_t i = start_frame; i <= end_frame && i < m_frames.size(); i++) {
            for (int c = 0; c < 4; c++) {
                m_frames[i].controller_inputs[c] = 0;
            }
        }
        invalidate_greenzone(start_frame);
    }

    void undo() override {
        if (!m_undo_stack.empty()) {
            m_redo_stack.push_back(m_frames);
            m_frames = m_undo_stack.back();
            m_undo_stack.pop_back();
        }
    }

    void redo() override {
        if (!m_redo_stack.empty()) {
            m_undo_stack.push_back(m_frames);
            m_frames = m_redo_stack.back();
            m_redo_stack.pop_back();
        }
    }

    bool can_undo() const override {
        return !m_undo_stack.empty();
    }

    bool can_redo() const override {
        return !m_redo_stack.empty();
    }

    void increment_rerecord_count() override {
        m_info.rerecord_count++;
    }

    void invalidate_greenzone(uint64_t from_frame) override {
        auto it = m_greenzone.lower_bound(from_frame);
        while (it != m_greenzone.end()) {
            it = m_greenzone.erase(it);
        }
    }

    bool has_greenzone_at(uint64_t frame) const override {
        return m_greenzone.count(frame) > 0;
    }

    bool seek_to_frame(uint64_t frame) override {
        if (frame >= m_frames.size()) return false;

        // Find closest greenzone state before target
        auto it = m_greenzone.upper_bound(frame);
        if (it != m_greenzone.begin()) {
            --it;
            if (m_host->load_state_from_buffer(it->second)) {
                m_current_frame = it->first;
                // Play forward to target frame
                while (m_current_frame < frame) {
                    for (int c = 0; c < 4; c++) {
                        m_host->set_controller_input(c, m_frames[m_current_frame].controller_inputs[c]);
                    }
                    m_host->frame_advance();
                    m_current_frame++;
                }
                increment_rerecord_count();
                return true;
            }
        }

        // Fall back to playing from start
        if (m_info.starts_from_savestate && !m_start_state.empty()) {
            m_host->load_state_from_buffer(m_start_state);
        } else {
            m_host->reset_emulator();
        }
        m_current_frame = 0;
        while (m_current_frame < frame) {
            for (int c = 0; c < 4; c++) {
                m_host->set_controller_input(c, m_frames[m_current_frame].controller_inputs[c]);
            }
            m_host->frame_advance();
            m_current_frame++;
        }
        increment_rerecord_count();
        return true;
    }

    void set_selection(uint64_t start, uint64_t end) override {
        m_selection_start = start;
        m_selection_end = end;
    }

    void get_selection(uint64_t& start, uint64_t& end) const override {
        start = m_selection_start;
        end = m_selection_end;
    }

    void copy_selection() override {
        m_clipboard.clear();
        for (uint64_t i = m_selection_start; i <= m_selection_end && i < m_frames.size(); i++) {
            m_clipboard.push_back(m_frames[i]);
        }
    }

    void cut_selection() override {
        copy_selection();
        save_undo_state();
        m_frames.erase(m_frames.begin() + m_selection_start,
                       m_frames.begin() + std::min(m_selection_end + 1, (uint64_t)m_frames.size()));
        // Renumber
        for (uint64_t i = m_selection_start; i < m_frames.size(); i++) {
            m_frames[i].frame_number = i;
        }
        invalidate_greenzone(m_selection_start);
    }

    void paste_at(uint64_t frame) override {
        if (m_clipboard.empty()) return;
        save_undo_state();
        m_frames.insert(m_frames.begin() + frame, m_clipboard.begin(), m_clipboard.end());
        // Renumber
        for (uint64_t i = frame; i < m_frames.size(); i++) {
            m_frames[i].frame_number = i;
        }
        invalidate_greenzone(frame);
    }

    uint64_t get_current_frame() const override {
        return m_current_frame;
    }

    uint64_t get_total_frames() const override {
        return m_frames.size();
    }

    uint64_t get_rerecord_count() const override {
        return m_info.rerecord_count;
    }

    void add_marker(uint64_t frame, const char* description) override {
        m_markers[frame] = description;
    }

    void remove_marker(uint64_t frame) override {
        m_markers.erase(frame);
    }

    int get_marker_count() const override {
        return static_cast<int>(m_markers.size());
    }

    uint64_t get_marker_frame(int index) const override {
        if (index < 0 || index >= static_cast<int>(m_markers.size())) return 0;
        auto it = m_markers.begin();
        std::advance(it, index);
        return it->first;
    }

    const char* get_marker_description(int index) const override {
        if (index < 0 || index >= static_cast<int>(m_markers.size())) return "";
        auto it = m_markers.begin();
        std::advance(it, index);
        return it->second.c_str();
    }

private:
    void save_undo_state() {
        m_undo_stack.push_back(m_frames);
        if (m_undo_stack.size() > 100) {
            m_undo_stack.pop_front();
        }
        m_redo_stack.clear();
    }

    bool load_fm2(const char* filename) {
        // Simple FM2 parser
        std::ifstream file(filename);
        if (!file) return false;

        m_frames.clear();
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            // Frame line format: |1|23456789|
            if (line[0] == '|') {
                emu::TASFrameData frame{};
                frame.frame_number = m_frames.size();

                // Parse controller 1
                if (line.length() >= 10) {
                    uint32_t buttons = 0;
                    if (line[3] == 'R') buttons |= 0x80;  // Right
                    if (line[4] == 'L') buttons |= 0x40;  // Left
                    if (line[5] == 'D') buttons |= 0x20;  // Down
                    if (line[6] == 'U') buttons |= 0x10;  // Up
                    if (line[7] == 'T') buttons |= 0x08;  // Start
                    if (line[8] == 'S') buttons |= 0x04;  // Select
                    if (line[9] == 'B') buttons |= 0x02;  // B
                    if (line[10] == 'A') buttons |= 0x01; // A
                    frame.controller_inputs[0] = buttons;
                }

                m_frames.push_back(frame);
            }
        }

        m_info.frame_count = m_frames.size();
        m_movie_loaded = true;
        m_mode = emu::TASMode::Stopped;
        return true;
    }

    emu::ITASHost* m_host = nullptr;
    std::string m_filename;
    emu::TASMovieInfo m_info{};
    std::vector<emu::TASFrameData> m_frames;
    std::vector<uint8_t> m_start_state;
    bool m_movie_loaded = false;
    emu::TASMode m_mode = emu::TASMode::Stopped;
    uint64_t m_current_frame = 0;

    // Greenzone (savestate snapshots)
    std::map<uint64_t, std::vector<uint8_t>> m_greenzone;

    // Undo/redo
    std::deque<std::vector<emu::TASFrameData>> m_undo_stack;
    std::deque<std::vector<emu::TASFrameData>> m_redo_stack;

    // Selection
    uint64_t m_selection_start = 0;
    uint64_t m_selection_end = 0;
    std::vector<emu::TASFrameData> m_clipboard;

    // Markers
    std::map<uint64_t, std::string> m_markers;
};

} // anonymous namespace

// C interface implementation
extern "C" {

EMU_PLUGIN_EXPORT emu::ITASPlugin* create_tas_plugin() {
    return new DefaultTASPlugin();
}

EMU_PLUGIN_EXPORT void destroy_tas_plugin(emu::ITASPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_tas_plugin_api_version() {
    return EMU_TAS_PLUGIN_API_VERSION;
}

} // extern "C"
