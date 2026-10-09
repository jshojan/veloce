#include "peer_connection.hpp"
#include "emu/netplay_plugin.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
using SockLength = int;
static void close_socket(Socket s) { closesocket(s); }
static int socket_error() { return WSAGetLastError(); }
static bool would_block(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
static bool interrupted(int e) { return e == WSAEINTR; }
static bool nonblocking(Socket s) { u_long value = 1; return ioctlsocket(s, FIONBIO, &value) == 0; }
struct SocketRuntime {
    SocketRuntime() { WSADATA data{}; if (WSAStartup(MAKEWORD(2,2), &data)) throw std::runtime_error("Winsock initialization failed"); }
    ~SocketRuntime() { WSACleanup(); }
};
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket invalid_socket = -1;
using SockLength = socklen_t;
static void close_socket(Socket s) { ::close(s); }
static int socket_error() { return errno; }
static bool would_block(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS; }
static bool interrupted(int e) { return e == EINTR; }
static bool nonblocking(Socket s) {
    int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
struct SocketRuntime {};
#endif

namespace emu {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t max_queue = 8192, max_payload = 64;
void put(std::vector<uint8_t>& data, uint64_t n, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) data.push_back(static_cast<uint8_t>(n >> (i * 8)));
}
uint64_t get(const std::vector<uint8_t>& data, size_t& offset, int bytes) {
    uint64_t n = 0;
    for (int i = 0; i < bytes; ++i) n = (n << 8) | data.at(offset++);
    return n;
}
bool same(const PeerIdentity& a, const PeerIdentity& b) {
    return a.core_id == b.core_id && a.build_id == b.build_id && a.first_frame == b.first_frame &&
        a.state_hash == b.state_hash && a.rom_hash == b.rom_hash &&
        a.rom_crc32 == b.rom_crc32 && a.core_abi == b.core_abi &&
        a.rollback_window == b.rollback_window;
}
}

struct PeerConnection::Impl {
    PeerIdentity identity;
    int player;
    State state = State::Idle;
    Socket listener = invalid_socket, socket = invalid_socket;
    uint16_t bound_port = 0;
    std::string error;
    std::vector<uint8_t> outgoing, incoming;
    size_t sent = 0;
    std::vector<PeerInput> inputs;
    bool hello_received = false;
    Clock::time_point last_receive = Clock::now(), last_send = Clock::now();
    explicit Impl(PeerIdentity id, int slot) : identity(id), player(slot) {
        static SocketRuntime runtime;
        (void)runtime;
        if (slot < 0 || slot > 1 || id.rollback_window == 0 || id.rollback_window > NETPLAY_MAX_ROLLBACK_FRAMES)
            throw std::invalid_argument("invalid peer role or rollback window");
    }
    ~Impl() { release(); }
    void release() {
        if (socket != invalid_socket) close_socket(socket);
        if (listener != invalid_socket) close_socket(listener);
        socket = listener = invalid_socket;
    }
    bool fail(const std::string& why) { error = why; state = State::Failed; release(); return false; }
    bool configure(Socket s) {
        int no_delay = 1;
        if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&no_delay), sizeof(no_delay)) != 0)
            return false;
#ifdef SO_NOSIGPIPE
        int enabled = 1;
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
#endif
        return nonblocking(s);
    }
    bool queue(const std::vector<uint8_t>& payload) {
        if (outgoing.size() + 4 + payload.size() > max_queue) return fail("send queue overflow");
        put(outgoing, payload.size(), 4);
        outgoing.insert(outgoing.end(), payload.begin(), payload.end());
        last_send = Clock::now();
        return true;
    }
    void begin_handshake() {
        state = State::Handshaking;
        last_receive = Clock::now();
        std::vector<uint8_t> hello{1};
        put(hello, 0x564E5031, 4); // VNP1 protocol identifier
        put(hello, identity.core_id, 8); put(hello, identity.build_id, 8);
        put(hello, identity.first_frame, 8); put(hello, identity.state_hash, 8);
        put(hello, identity.rom_hash, 8);
        put(hello, identity.rom_crc32, 4); put(hello, identity.core_abi, 4);
        put(hello, identity.rollback_window, 4); put(hello, player, 1);
        queue(hello);
    }
    bool parse(const std::vector<uint8_t>& payload) {
        if (payload[0] == 1) {
            if (state != State::Handshaking || hello_received || payload.size() != 58)
                return fail("invalid or duplicate hello");
            size_t offset = 1;
            if (get(payload, offset, 4) != 0x564E5031) return fail("protocol mismatch");
            PeerIdentity other;
            other.core_id = get(payload, offset, 8); other.build_id = get(payload, offset, 8);
            other.first_frame = get(payload, offset, 8); other.state_hash = get(payload, offset, 8);
            other.rom_hash = get(payload, offset, 8);
            other.rom_crc32 = static_cast<uint32_t>(get(payload, offset, 4));
            other.core_abi = static_cast<uint32_t>(get(payload, offset, 4));
            other.rollback_window = static_cast<uint32_t>(get(payload, offset, 4));
            auto remote_player = get(payload, offset, 1);
            if (!same(identity, other) || remote_player != static_cast<uint64_t>(1 - player))
                return fail("ROM/core/build/start-state/role mismatch");
            hello_received = true;
            queue({2}); // Ack means this side validated the hello.
        } else if (payload[0] == 2) {
            if (state != State::Handshaking || !hello_received || payload.size() != 1)
                return fail("unexpected handshake ack");
            state = State::Ready;
        } else if (payload[0] == 3) {
            if (state != State::Ready || payload.size() != 1) return fail("unexpected heartbeat");
        } else if (payload[0] == 4) {
            if (state != State::Ready || payload.size() != 13 || inputs.size() >= 64)
                return fail("invalid input message or input queue overflow");
            size_t offset = 1;
            uint64_t frame = get(payload, offset, 8);
            inputs.push_back({frame, static_cast<uint32_t>(get(payload, offset, 4))});
        } else return fail("unknown message type");
        last_receive = Clock::now();
        return state != State::Failed;
    }
    bool parse_incoming() {
        while (incoming.size() >= 4) {
            size_t offset = 0;
            auto size = get(incoming, offset, 4);
            if (size == 0 || size > max_payload) return fail("invalid message length");
            if (incoming.size() < 4 + size) return true;
            std::vector<uint8_t> payload(incoming.begin() + 4, incoming.begin() + 4 + size);
            incoming.erase(incoming.begin(), incoming.begin() + 4 + size);
            if (!parse(payload)) return false;
        }
        return true;
    }
    void flush() {
        // Bound work per poll so gameplay cannot be held by continuous traffic.
        for (int attempt = 0; attempt < 16 && sent < outgoing.size(); ++attempt) {
            int flags = 0;
#ifdef MSG_NOSIGNAL
            flags = MSG_NOSIGNAL;
#endif
            int n = static_cast<int>(::send(socket, reinterpret_cast<const char*>(outgoing.data() + sent),
                                           static_cast<int>(outgoing.size() - sent), flags));
            if (n > 0) sent += static_cast<size_t>(n);
            else if (n < 0 && interrupted(socket_error())) continue;
            else if (n < 0 && would_block(socket_error())) break;
            else { fail("socket send failed"); return; }
        }
        if (sent == outgoing.size()) { outgoing.clear(); sent = 0; }
    }
    void receive() {
        std::array<char, 1024> bytes{};
        for (int attempt = 0; attempt < 8; ++attempt) {
            int n = static_cast<int>(::recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0));
            if (n > 0) {
                if (incoming.size() + static_cast<size_t>(n) > max_queue) { fail("receive queue overflow"); return; }
                incoming.insert(incoming.end(), bytes.begin(), bytes.begin() + n);
                if (!parse_incoming()) return;
            } else if (n == 0) {
                if (!incoming.empty()) fail("peer closed with truncated message");
                else { state = State::Closed; error = "peer closed"; release(); }
                return;
            } else if (interrupted(socket_error())) continue;
            else if (would_block(socket_error())) return;
            else { fail("socket receive failed"); return; }
        }
    }
};

PeerConnection::PeerConnection(PeerIdentity identity, int player) : m(new Impl(identity, player)) {}
PeerConnection::~PeerConnection() = default;
bool PeerConnection::listen(const std::string& ipv4, uint16_t port) {
    if (m->state != State::Idle || m->player != 0) return false;
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv4.c_str(), &address.sin_addr) != 1) return m->fail("invalid IPv4 bind address");
    m->listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m->listener == invalid_socket || !m->configure(m->listener)) return m->fail("socket creation failed");
    if (::bind(m->listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(m->listener, 1) != 0) return m->fail("socket bind/listen failed");
    SockLength length = sizeof(address);
    if (getsockname(m->listener, reinterpret_cast<sockaddr*>(&address), &length) != 0) return m->fail("socket address query failed");
    m->bound_port = ntohs(address.sin_port); m->state = State::Listening;
    return true;
}
bool PeerConnection::connect(const std::string& ipv4, uint16_t port) {
    if (m->state != State::Idle || m->player != 1) return false;
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv4.c_str(), &address.sin_addr) != 1 || port == 0) return m->fail("invalid IPv4 peer address/port");
    m->socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m->socket == invalid_socket || !m->configure(m->socket)) return m->fail("socket creation failed");
    int result = ::connect(m->socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result == 0) m->begin_handshake();
    else if (would_block(socket_error())) { m->state = State::Connecting; m->last_receive = Clock::now(); }
    else return m->fail("socket connect failed");
    return true;
}
void PeerConnection::poll() {
    if (m->state == State::Listening) {
        Socket accepted = ::accept(m->listener, nullptr, nullptr);
        if (accepted == invalid_socket) {
            if (!would_block(socket_error()) && !interrupted(socket_error())) m->fail("socket accept failed");
            return;
        }
        m->socket = accepted; close_socket(m->listener); m->listener = invalid_socket;
        if (!m->configure(accepted)) { m->fail("accepted socket configuration failed"); return; }
        m->begin_handshake();
    }
    if (m->state == State::Connecting) {
#ifdef _WIN32
        fd_set writable, errors; FD_ZERO(&writable); FD_ZERO(&errors);
        FD_SET(m->socket, &writable); FD_SET(m->socket, &errors);
        timeval timeout{};
        int result = select(0, nullptr, &writable, &errors, &timeout);
#else
        pollfd descriptor{m->socket, POLLOUT, 0};
        int result = ::poll(&descriptor, 1, 0);
#endif
        if (result < 0) { if (!interrupted(socket_error())) m->fail("connect poll failed"); return; }
        if (result > 0) {
            int error = 0; SockLength size = sizeof(error);
            if (getsockopt(m->socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0 || error != 0) {
                m->fail("asynchronous socket connect failed"); return;
            }
            m->begin_handshake();
        }
    }
    auto now = Clock::now();
    if ((m->state == State::Connecting || m->state == State::Handshaking || m->state == State::Ready) &&
        now - m->last_receive > std::chrono::seconds(10)) { m->fail("peer timeout"); return; }
    if (m->state != State::Handshaking && m->state != State::Ready) return;
    if (m->state == State::Ready && now - m->last_send > std::chrono::seconds(1)) m->queue({3});
    if (m->state == State::Failed) return;
    m->flush();
    if (m->state != State::Failed) m->receive();
}
bool PeerConnection::send_input(uint64_t frame, uint32_t buttons) {
    if (m->state != State::Ready) return false;
    std::vector<uint8_t> payload{4}; put(payload, frame, 8); put(payload, buttons, 4);
    return m->queue(payload);
}
std::vector<PeerInput> PeerConnection::take_inputs() { std::vector<PeerInput> result; result.swap(m->inputs); return result; }
bool PeerConnection::has_pending_output() const { return m->sent < m->outgoing.size(); }
void PeerConnection::close() {
    m->release();
    m->outgoing.clear(); m->incoming.clear(); m->inputs.clear(); m->sent = 0;
    m->state = State::Closed;
}
PeerConnection::State PeerConnection::state() const { return m->state; }
uint16_t PeerConnection::port() const { return m->bound_port; }
const std::string& PeerConnection::error() const { return m->error; }
} // namespace emu
