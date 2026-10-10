#pragma once

#include "emu/netplay_plugin.hpp"
#include <map>
#include <vector>

namespace emu {

// Owns two-peer frame history (one host-owned controller or two controllers) and deterministic replay. A transport submits
// remote inputs; the caller advances local frames and handles stale/conflicting
// packets or failed core serialization. This class never opens sockets.
class RollbackSession {
public:
    enum class Result { Advanced, Waiting, Accepted, TooOld, TooFarAhead, Conflict, Failed };
    RollbackSession(INetplayCapable& core, int local_player, size_t window, uint64_t first_frame = 0);
    Result advance(uint32_t local_input);
    Result submit_remote(uint64_t frame, uint32_t buttons);
    uint64_t next_frame() const { return m_next_frame; }
    uint64_t rollback_count() const { return m_rollback_count; }
    size_t last_rollback_depth() const { return m_last_depth; }
    bool failed() const { return m_failed; }
    // Streaming-audio hosts must suppress output while this is true.
    bool is_replaying() const { return m_replaying; }

private:
    struct Frame {
        uint64_t id = 0;
        bool valid = false;
        bool remote_confirmed = false;
        uint32_t local = 0, remote = 0, previous_remote = 0;
        std::vector<uint8_t> state;
        size_t size = 0;
    };
    Frame* find(uint64_t id);
    bool save(Frame& frame);
    void execute(const Frame& frame);
    Result replay(uint64_t first);
    INetplayCapable& m_core;
    int m_local_player;
    size_t m_window;
    uint64_t m_first_frame, m_next_frame;
    uint32_t m_last_remote = 0;
    std::vector<Frame> m_frames;
    std::map<uint64_t, uint32_t> m_pending;
    uint64_t m_rollback_count = 0;
    size_t m_last_depth = 0;
    bool m_failed = false;
    bool m_replaying = false;
};
} // namespace emu
