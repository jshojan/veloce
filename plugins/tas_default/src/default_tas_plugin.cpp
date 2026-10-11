#include "emu/tas_plugin.hpp"
#include "movie_file.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <deque>
#include <map>
#include <cctype>

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
            "2.0.0",
            "Veloce Team",
            "Tool-assisted speedrun movie editor and player. Features greenzone "
            "savestate snapshots, undo/redo support, frame editing, and FM2 import.",
            s_file_formats
        };
    }

    bool initialize(emu::ITASHost* host) override {
        if (!host) return false;
        m_host = host;
        return true;
    }

    void shutdown() override {
        close_movie();
        m_host = nullptr;
    }

    bool new_movie(const char* filename, bool from_savestate) override {
        if (!m_host || !filename || !*filename || fm2_path(filename)) return false;
        std::vector<uint8_t> start;
        if (from_savestate && (!m_host->save_state_to_buffer(start) || start.empty())) return false;
        close_movie();

        m_filename = filename;
        m_info = {};
        std::strncpy(m_info.platform, m_host->get_platform_name(), sizeof(m_info.platform) - 1);
        std::strncpy(m_info.rom_name, m_host->get_rom_name(), sizeof(m_info.rom_name) - 1);
        m_info.rom_crc32 = m_host->get_rom_crc32();
        m_info.starts_from_savestate = from_savestate;
        m_info.frame_count = 0;
        m_info.rerecord_count = 0;

        m_start_state = std::move(start);
        if (!from_savestate) m_host->reset_emulator();

        m_frames.clear();
        m_movie_loaded = true;
        m_mode = emu::TASMode::Recording;

        return true;
    }

    bool open_movie(const char* filename) override {
        emu::tas::Movie movie;
        if (!emu::tas::load_movie_file(filename, movie)) return false;
        close_movie();
        m_info = movie.info;
        m_frames = std::move(movie.frames);
        m_start_state = std::move(movie.start_state);
        m_filename = filename;
        m_imported_fm2 = movie.format == emu::tas::MovieFormat::FM2;
        m_movie_loaded = true;
        return true;
    }

    bool save_movie() override { return !m_imported_fm2 && save_movie_as(m_filename.c_str()); }
    bool save_movie_as(const char* filename) override {
        if (!m_movie_loaded || !filename || fm2_path(filename)) return false;
        emu::tas::Movie movie{m_info,m_start_state,m_frames};
        if (!emu::tas::save_movie_file(filename, movie)) return false;
        m_info.frame_count = m_frames.size();
        m_filename = filename;
        m_imported_fm2 = false;
        return true;
    }

    void close_movie() override {
        m_info = {};
        m_frames.clear();
        m_start_state.clear();
        m_greenzone.clear();
        m_markers.clear();
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_movie_loaded = false;
        m_imported_fm2 = false;
        m_mode = emu::TASMode::Stopped;
        m_current_frame = 0;
        m_sample_valid = false;
    }

    bool is_movie_loaded() const override {
        return m_movie_loaded;
    }

    emu::TASMovieInfo get_movie_info() const override {
        return m_info;
    }

    void set_movie_info(const emu::TASMovieInfo& info) override {
        m_info = info;
        m_info.title[sizeof(m_info.title)-1] = 0;
        m_info.author[sizeof(m_info.author)-1] = 0;
        m_info.description[sizeof(m_info.description)-1] = 0;
        m_info.platform[sizeof(m_info.platform)-1] = 0;
        m_info.rom_name[sizeof(m_info.rom_name)-1] = 0;
        m_info.frame_count = m_frames.size();
        m_info.starts_from_savestate = !m_start_state.empty();
    }

    emu::TASMode get_mode() const override {
        return m_mode;
    }

    void set_mode(emu::TASMode mode) override {
        if (m_movie_loaded || mode == emu::TASMode::Stopped) { m_mode = mode; m_sample_valid = false; }
    }

    void start_recording() override {
        if (!compatible_rom() || !m_movie_loaded) return;
        if (m_info.starts_from_savestate) {
            if (!m_host->load_state_from_buffer(m_start_state)) return;
        } else m_host->reset_emulator();
        m_mode = emu::TASMode::Recording;
        m_current_frame = 0;
        m_frames.clear();
        m_info.frame_count = 0;
        m_greenzone.clear();
        m_sample_valid = false;
    }
    void stop_recording() override { m_mode = emu::TASMode::Stopped; m_info.frame_count = m_frames.size(); m_sample_valid = false; }
    void start_playback() override {
        if (!compatible_rom() || !m_movie_loaded || m_frames.empty()) { m_mode = emu::TASMode::Stopped; return; }
        if (m_info.starts_from_savestate) {
            if (!m_host->load_state_from_buffer(m_start_state)) return;
        } else m_host->reset_emulator();
        m_mode = emu::TASMode::Playing;
        m_current_frame = 0;
        m_sample_valid = false;
    }
    void stop_playback() override { m_mode = emu::TASMode::Stopped; m_sample_valid = false; }

    uint32_t on_frame(int controller) override {
        if (!m_host || controller < 0 || controller >= 4) return 0;
        if (!m_movie_loaded || m_mode == emu::TASMode::Stopped) return m_host->get_controller_input(controller);
        if (!m_sample_valid) {
            if (m_mode == emu::TASMode::Recording) {
                if (m_current_frame >= 1000000) { m_mode = emu::TASMode::Stopped; return m_host->get_controller_input(controller); }
                for (int c = 0; c < 4; ++c) m_sample.controller_inputs[c] = m_host->get_controller_input(c);
                m_sample.frame_number = m_current_frame;
                m_sample.has_reset = false; m_sample.has_savestate = false; m_sample.savestate_slot = 0;
            } else if (m_current_frame < m_frames.size()) m_sample = m_frames[m_current_frame];
            else { m_mode = emu::TASMode::Stopped; return m_host->get_controller_input(controller); }
            m_sample_valid = true;
        }
        return m_sample.controller_inputs[controller];
    }
    void on_frame_completed() override {
        if (!m_sample_valid) return;
        if (m_mode == emu::TASMode::Recording) {
            if (m_current_frame < m_frames.size()) m_frames[m_current_frame] = m_sample;
            else m_frames.push_back(m_sample);
            m_info.frame_count = m_frames.size();
        }
        ++m_current_frame;
        m_sample_valid = false;
        // This snapshot represents the boundary after N executed movie frames.
        if (m_current_frame % 60 == 0) {
            std::vector<uint8_t> state;
            if (m_host->save_state_to_buffer(state) && !state.empty()) m_greenzone[m_current_frame] = std::move(state);
        }
        if (m_mode != emu::TASMode::Recording && m_current_frame >= m_frames.size()) m_mode = emu::TASMode::Stopped;
    }

    emu::TASFrameData get_frame(uint64_t frame) const override {
        if (frame < m_frames.size()) {
            return m_frames[frame];
        }
        return {};
    }

    void set_frame(uint64_t frame, const emu::TASFrameData& data) override {
        if (m_mode != emu::TASMode::ReadOnly && frame < m_frames.size()) {
            save_undo_state();
            m_frames[frame] = data;
            m_frames[frame].frame_number = frame;
            invalidate_greenzone(frame);
        }
    }

    void insert_frame(uint64_t after_frame) override {
        if (m_mode == emu::TASMode::ReadOnly || m_frames.size() >= 1000000) return;
        after_frame = m_frames.empty() ? 0 : std::min(after_frame, uint64_t(m_frames.size()-1));
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
        if (m_frames.size()==1) m_frames[0].frame_number=0;
        invalidate_greenzone(m_frames.size()==1?0:after_frame+1);
    }

    void delete_frame(uint64_t frame) override {
        if (m_mode != emu::TASMode::ReadOnly && frame < m_frames.size()) {
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
        if (m_mode == emu::TASMode::ReadOnly || start_frame > end_frame || start_frame >= m_frames.size()) return;
        save_undo_state();
        for (uint64_t i = start_frame; i <= end_frame && i < m_frames.size(); i++) {
            for (int c = 0; c < 4; c++) {
                m_frames[i].controller_inputs[c] = 0;
            }
        }
        invalidate_greenzone(start_frame);
    }

    void undo() override {
        if (m_mode == emu::TASMode::ReadOnly) return;
        if (!m_undo_stack.empty()) {
            m_redo_stack.push_back(m_frames);
            m_frames = m_undo_stack.back();
            m_undo_stack.pop_back();
            invalidate_greenzone(0);
            m_info.frame_count = m_frames.size();
            m_sample_valid = false;
        }
    }

    void redo() override {
        if (m_mode == emu::TASMode::ReadOnly) return;
        if (!m_redo_stack.empty()) {
            m_undo_stack.push_back(m_frames);
            m_frames = m_redo_stack.back();
            m_redo_stack.pop_back();
            invalidate_greenzone(0);
            m_info.frame_count = m_frames.size();
            m_sample_valid = false;
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
        m_info.frame_count = m_frames.size();
        m_sample_valid = false;
        auto it = m_greenzone.upper_bound(from_frame);
        while (it != m_greenzone.end()) {
            it = m_greenzone.erase(it);
        }
    }

    bool has_greenzone_at(uint64_t frame) const override {
        return m_greenzone.count(frame) > 0;
    }

    bool seek_to_frame(uint64_t frame) override {
        if (!compatible_rom() || !m_movie_loaded || frame > m_frames.size()) return false;
        std::vector<uint8_t> backup;
        if (!m_host->save_state_to_buffer(backup) || backup.empty()) return false;
        uint64_t position = 0;
        auto it = m_greenzone.upper_bound(frame);
        bool restored = false;
        if (it != m_greenzone.begin()) { --it; position = it->first; restored = m_host->load_state_from_buffer(it->second); }
        else if (m_info.starts_from_savestate) restored = m_host->load_state_from_buffer(m_start_state);
        else { m_host->reset_emulator(); restored = true; }
        if (!restored) { m_host->load_state_from_buffer(backup); return false; }
        for (; position < frame; ++position) {
            if (!m_host->run_replay_frame(m_frames[position])) {
                m_host->load_state_from_buffer(backup);
                return false;
            }
        }
        m_current_frame = frame;
        m_sample_valid = false;
        m_mode = emu::TASMode::Stopped;
        m_host->pause_emulator();
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
        if (m_mode == emu::TASMode::ReadOnly || m_selection_start > m_selection_end || m_selection_start >= m_frames.size()) return;
        m_selection_end = std::min(m_selection_end,uint64_t(m_frames.size()-1));
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
        if (m_mode == emu::TASMode::ReadOnly || m_clipboard.empty() || m_frames.size()+m_clipboard.size()>1000000) return;
        frame = std::min(frame,uint64_t(m_frames.size()));
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
    bool compatible_rom() const {
        if (!m_host || (m_info.rom_crc32 && m_info.rom_crc32 != m_host->get_rom_crc32())) return false;
        return !m_info.platform[0] || (m_host->get_platform_name() && std::strcmp(m_info.platform,m_host->get_platform_name())==0);
    }
    static bool fm2_path(const char* filename) {
        std::string name(filename);
        if (name.size() < 4) return false;
        auto suffix=name.substr(name.size()-4);
        for(auto& ch:suffix)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return suffix==".fm2";
    }
    void save_undo_state() {
        m_undo_stack.push_back(m_frames);
        if (m_undo_stack.size() > 100) {
            m_undo_stack.pop_front();
        }
        m_redo_stack.clear();
    }

    emu::ITASHost* m_host = nullptr;
    std::string m_filename;
    emu::TASMovieInfo m_info{};
    std::vector<emu::TASFrameData> m_frames;
    std::vector<uint8_t> m_start_state;
    bool m_movie_loaded = false;
    bool m_imported_fm2 = false;
    emu::TASMode m_mode = emu::TASMode::Stopped;
    uint64_t m_current_frame = 0;
    emu::TASFrameData m_sample{};
    bool m_sample_valid = false;

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
