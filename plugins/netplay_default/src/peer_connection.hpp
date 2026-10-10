#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace emu {
struct PeerIdentity {
    uint64_t core_id = 0, build_id = 0, first_frame = 0, state_hash = 0, rom_hash = 0;
    uint32_t rom_crc32 = 0, core_abi = 0, rollback_window = 0;
};
struct PeerInput { uint64_t frame; uint32_t buttons; };

// Nonblocking, bounded, two-peer IPv4 TCP transport. Poll on the emulator
// thread, including while paused. A hello/ack exchange validates compatibility
// before input messages are allowed. It is not an authenticated Internet relay.
class PeerConnection {
public:
    enum class State { Idle, Listening, Connecting, Handshaking, Ready, Closed, Failed };
    PeerConnection(PeerIdentity identity, int local_player);
    ~PeerConnection();
    PeerConnection(const PeerConnection&) = delete;
    PeerConnection& operator=(const PeerConnection&) = delete;
    bool listen(const std::string& ipv4, uint16_t port);
    bool connect(const std::string& ipv4, uint16_t port);
    void poll();
    bool send_input(uint64_t frame, uint32_t buttons);
    bool has_pending_output() const;
    std::vector<PeerInput> take_inputs();
    void close();
    State state() const;
    uint16_t port() const;
    const std::string& error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
} // namespace emu
