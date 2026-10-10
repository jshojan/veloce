#include "nes_plugin_fixture.hpp"
#include "peer_connection.hpp"
#include "rollback_session.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <thread>

using emulator_test::require;
static uint64_t hash_bytes(const std::vector<uint8_t>& data) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (auto byte : data) hash = (hash ^ byte) * 0x100000001b3ULL;
    return hash;
}
static std::vector<uint8_t> read_bytes(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "fixture file missing");
    return {std::istreambuf_iterator<char>(stream), {}};
}
static uint32_t input(int player, uint64_t frame) {
    return static_cast<uint32_t>((frame * (player ? 13 : 7) + (player ? 1 : 3)) & 255);
}
int main(int argc, char** argv) {
    try {
        require(argc == 8, "expected role, core, ROM, output, port-file, port, and mismatch flag");
        std::string role = argv[1];
        emulator_test::LoadedCore fixture(argv[2], read_bytes(argv[3]));
        auto& core = fixture.netplay();
        const bool mismatch = std::string(argv[7]) == "1";
        uint64_t rollbacks = 0;
        if (role == "baseline") {
            for (uint64_t frame = 0; frame < 100; ++frame) core.run_frame_netplay(input(0, frame), input(1, frame));
        } else {
            int local = role == "host" ? 0 : 1;
            emu::PeerIdentity identity;
            identity.core_id = 0x4E4553; // NES
            identity.build_id = hash_bytes(read_bytes(argv[2]));
            identity.state_hash = hash_bytes(emulator_test::snapshot(core));
            identity.rom_hash = hash_bytes(read_bytes(argv[3])) ^ (mismatch && local == 1 ? 1 : 0);
            identity.rom_crc32 = fixture.plugin->get_rom_crc32();
            identity.core_abi = EMU_PLUGIN_API_VERSION;
            identity.rollback_window = 8;
            emu::PeerConnection peer(identity, local);
            emu::RollbackSession execution(core, local, 8);
            if (local == 0) {
                if (!peer.listen("127.0.0.1", 0)) throw std::runtime_error(peer.error());
                auto temporary_port = std::string(argv[5]) + ".tmp";
                std::ofstream port_file(temporary_port);
                port_file << peer.port() << "\n";
                port_file.flush();
                require(port_file.good(), "could not publish local test port");
                port_file.close();
                std::filesystem::rename(temporary_port, argv[5]);
            } else if (!peer.connect("127.0.0.1", static_cast<uint16_t>(std::stoul(argv[6]))))
                throw std::runtime_error(peer.error());
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            uint64_t remote_received = 0;
            std::optional<uint64_t> sent_frame;
            uint32_t pinned_input = 0;
            uint64_t tick = 0;
            while (std::chrono::steady_clock::now() < deadline) {
                // Poll in bursts to exercise delayed confirmations over actual
                // sockets; the two processes have independent emulator state.
                if (++tick % (local ? 5 : 3) == 0 || peer.state() != emu::PeerConnection::State::Ready)
                    peer.poll();
                for (const auto& message : peer.take_inputs()) {
                    require(message.frame == remote_received, "remote stream frame order changed");
                    require(execution.submit_remote(message.frame, message.buttons) == emu::RollbackSession::Result::Accepted,
                            "remote input rejected by rollback module");
                    ++remote_received;
                }
                if (peer.state() == emu::PeerConnection::State::Failed) {
                    if (mismatch && peer.error().find("mismatch") != std::string::npos) {
                        std::cout << "EXPECTED_HANDSHAKE_REJECTED " << peer.error() << "\n";
                        return 42;
                    }
                    throw std::runtime_error(peer.error());
                }
                if (execution.next_frame() == 100 && remote_received == 100 && !peer.has_pending_output()) break;
                require(peer.state() != emu::PeerConnection::State::Closed, "peer closed before confirmation drain");
                if (peer.state() == emu::PeerConnection::State::Ready && execution.next_frame() < 100) {
                    auto frame = execution.next_frame();
                    if (!sent_frame) {
                        pinned_input = input(local, frame);
                        sent_frame = frame;
                    }
                    auto result = execution.advance(pinned_input);
                    require(result == emu::RollbackSession::Result::Advanced || result == emu::RollbackSession::Result::Waiting,
                            "network execution failed");
                    if (result == emu::RollbackSession::Result::Advanced) {
                        if (!peer.send_input(frame, pinned_input)) throw std::runtime_error(peer.error());
                        sent_frame.reset();
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            require(!mismatch, "incompatible peers completed a handshake");
            require(execution.next_frame() == 100 && remote_received == 100, "network session timed out");
            rollbacks = execution.rollback_count();
            require(rollbacks > 0, "network test did not exercise late-input replay");
        }
        std::filesystem::path output(argv[4]);
        auto state = emulator_test::snapshot(core);
        std::ofstream state_file(output.string() + ".state", std::ios::binary);
        state_file.write(reinterpret_cast<const char*>(state.data()), state.size());
        state_file.flush();
        require(state_file.good(), "could not persist network state");
        std::ofstream registers(output.string() + ".result");
        registers << "#VELOCE NETWORK 1 core=NES frames=100 role=" << role << "\n"
                  << "REG p0_sum=" << unsigned(fixture.plugin->read_memory(0x12))
                  << " p1_sum=" << unsigned(fixture.plugin->read_memory(0x13)) << "\n"
                  << "INFO rollbacks=" << rollbacks << " state_hash=" << hash_bytes(state) << "\n";
        registers.flush();
        require(registers.good(), "could not persist network register observations");
        std::cout << "COMPLETE " << role << " frames=100 rollbacks=" << rollbacks << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
