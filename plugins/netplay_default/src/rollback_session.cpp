#include "rollback_session.hpp"
#include <limits>
#include <stdexcept>
#include <new>

namespace emu {
RollbackSession::RollbackSession(INetplayCapable& core, int local_player, size_t window,
                                 uint64_t first_frame)
    : m_core(core), m_local_player(local_player), m_window(window), m_max_state_size(core.get_max_state_size()),
      m_first_frame(first_frame), m_next_frame(first_frame) {
    if (!core.is_deterministic() || core.get_max_players() < 1 ||
        local_player < 0 || local_player > 1 || window == 0 ||
        window > NETPLAY_MAX_ROLLBACK_FRAMES || m_max_state_size == 0)
        throw std::invalid_argument("rollback needs deterministic controller state and a valid window");
    m_frames.resize(window + 1);
}

RollbackSession::Frame* RollbackSession::find(uint64_t id) {
    auto& frame = m_frames[id % m_frames.size()];
    return frame.valid && frame.id == id ? &frame : nullptr;
}

bool RollbackSession::save(Frame& frame) {
    try {
        const size_t required = m_core.save_state_fast(nullptr, 0);
        if (!required || required > m_max_state_size) { m_failed = true; return false; }
        // Reuse each slot's capacity, growing only when serialized state grows.
        if (frame.state.size() < required) frame.state.resize(required);
        frame.size = m_core.save_state_fast(frame.state.data(), frame.state.size());
        if (!frame.size || frame.size > required) { m_failed = true; return false; }
    } catch (const std::bad_alloc&) { m_failed = true; return false; }
      catch (const std::length_error&) { m_failed = true; return false; }
    return true;
}

void RollbackSession::execute(const Frame& frame) {
    if (m_local_player == 0) m_core.run_frame_netplay(frame.local, frame.remote);
    else m_core.run_frame_netplay(frame.remote, frame.local);
}

RollbackSession::Result RollbackSession::advance(uint32_t local_input) {
    if (m_core.get_max_players()==1 && m_local_player==1) local_input=0;
    if (m_next_frame == std::numeric_limits<uint64_t>::max()) m_failed = true;
    if (m_failed) return Result::Failed;
    // Never execute more than window speculative frames past an unconfirmed
    // input, even when newer frames have already been confirmed out of order.
    uint64_t oldest = m_next_frame - m_first_frame > m_window ? m_next_frame - m_window : m_first_frame;
    for (uint64_t id = oldest; id < m_next_frame; ++id) {
        auto* prior = find(id);
        if (prior && !prior->remote_confirmed && m_next_frame - id >= m_window)
            return Result::Waiting;
    }
    auto& frame = m_frames[m_next_frame % m_frames.size()];
    frame.valid = false;
    frame.id = m_next_frame;
    frame.local = local_input;
    frame.previous_remote = m_last_remote;
    auto pending = m_pending.find(m_next_frame);
    frame.remote_confirmed = pending != m_pending.end();
    frame.remote = frame.remote_confirmed ? pending->second : m_last_remote;
    if (!save(frame)) return Result::Failed;
    if (pending != m_pending.end()) m_pending.erase(pending);
    frame.valid = true;
    execute(frame);
    m_last_remote = frame.remote;
    ++m_next_frame;
    return Result::Advanced;
}

RollbackSession::Result RollbackSession::submit_remote(uint64_t id, uint32_t buttons) {
    if (m_failed) return Result::Failed;
    if (m_core.get_max_players()==1 && m_local_player==0 && buttons) return Result::Conflict;
    if (id >= m_next_frame) {
        if (id - m_next_frame > m_window) return Result::TooFarAhead;
        auto [entry, inserted] = m_pending.emplace(id, buttons);
        return inserted || entry->second == buttons ? Result::Accepted : Result::Conflict;
    }
    if (id < m_first_frame || m_next_frame - id > m_window) return Result::TooOld;
    auto* frame = find(id);
    if (!frame) return Result::TooOld;
    if (frame->remote_confirmed)
        return frame->remote == buttons ? Result::Accepted : Result::Conflict;
    bool changed = frame->remote != buttons;
    frame->remote = buttons;
    frame->remote_confirmed = true;
    return changed ? replay(id) : Result::Accepted;
}

RollbackSession::Result RollbackSession::replay(uint64_t first) {
    m_replaying = true;
    struct ReplayGuard {
        bool& flag;
        ~ReplayGuard() { flag = false; }
    } guard{m_replaying};
    auto* initial = find(first);
    if (!initial || !m_core.load_state_fast(initial->state.data(), initial->size)) {
        m_failed = true;
        return Result::Failed;
    }
    uint32_t remote = initial->previous_remote;
    for (uint64_t id = first; id < m_next_frame; ++id) {
        auto* frame = find(id);
        if (!frame) { m_failed = true; return Result::Failed; }
        frame->previous_remote = remote;
        if (!frame->remote_confirmed) frame->remote = remote;
        if (!save(*frame)) return Result::Failed;
        execute(*frame);
        m_core.discard_audio();
        remote = frame->remote;
    }
    m_last_remote = remote;
    m_last_depth = static_cast<size_t>(m_next_frame - first);
    ++m_rollback_count;
    return Result::Accepted;
}
} // namespace emu
