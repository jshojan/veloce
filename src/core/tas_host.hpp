#pragma once
#include "emu/tas_plugin.hpp"
namespace emu {
class Application;
class TASHost final : public ITASHost {
public:
    explicit TASHost(Application& app):m_app(app){}
    void capture_reset_state();
    bool has_reset_state() const { return !m_reset_state.empty(); }
    void reset_emulator() override;
    void pause_emulator() override;void resume_emulator() override;void frame_advance() override;
    bool is_emulator_paused() const override;
    uint64_t get_current_frame() const override;double get_fps() const override;
    void set_controller_input(int controller,uint32_t buttons) override;
    uint32_t get_controller_input(int controller) const override;
    bool save_state(int slot) override;bool load_state(int slot) override;
    bool save_state_to_buffer(std::vector<uint8_t>& buffer) override;
    bool load_state_from_buffer(const std::vector<uint8_t>& buffer) override;
    bool run_replay_frame(const TASFrameData& frame) override;
    const char* get_rom_name() const override;uint32_t get_rom_crc32() const override;const char* get_platform_name() const override;
    uint8_t read_memory(uint16_t address) override;void write_memory(uint16_t address,uint8_t value) override;
private:
    Application& m_app;
    std::vector<uint8_t> m_reset_state;
};
}
