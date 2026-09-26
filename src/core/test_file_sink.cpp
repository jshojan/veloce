#include "test_file_sink.hpp"

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace emu {

namespace {

bool env_flag(const char* name, bool fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    return v[0] != '0';
}

// Header / note tokens are space-separated key=value pairs; keep them to one
// token.
std::string token_safe(const char* s) {
    std::string out;
    if (!s) return out;
    for (const char* p = s; *p; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  c == '_' || c == '-' || c == '.' || c == ',' || c == ':' || c == '/';
        out.push_back(ok ? static_cast<char>(c) : '_');
    }
    return out;
}

std::string key_safe(const char* s) {
    std::string out;
    if (s) {
        for (const char* p = s; *p; ++p) {
            unsigned char c = static_cast<unsigned char>(*p);
            bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      c == '_' || c == '-' || c == '.';
            out.push_back(ok ? static_cast<char>(c) : '_');
        }
    }
    return out.empty() ? std::string("note") : out;
}

std::string value_safe(const char* s) {
    std::string out;
    if (!s) return out;
    for (const char* p = s; *p && out.size() < TestFileSink::kMaxLineBytes; ++p) {
        unsigned char c = static_cast<unsigned char>(*p);
        out.push_back((c < 0x20 || c >= 0x7F) ? ' ' : static_cast<char>(c));
    }
    return out;
}

// "END <pass>/<total> [code=<n>]" -> status for finish(). Returns false if the
// line is not an END line.
bool parse_end_line(const std::string& line, int32_t& status) {
    if (line.compare(0, 3, "END") != 0) return false;
    if (line.size() > 3 && line[3] != ' ') return false;
    long pass = -1, total = -1, code = -1;
    const char* p = line.c_str() + 3;
    while (*p == ' ') ++p;
    char* end = nullptr;
    long a = std::strtol(p, &end, 10);
    if (end != p && *end == '/') {
        const char* q = end + 1;
        long b = std::strtol(q, &end, 10);
        if (end != q) { pass = a; total = b; }
    }
    const char* c = std::strstr(line.c_str(), " code=");
    if (c) {
        const char* q = c + 6;
        long v = std::strtol(q, &end, 0);
        if (end != q) code = v;
    }
    if (code >= 0) status = static_cast<int32_t>(code);
    else if (total >= 0 && pass == total) status = 0;   // includes END 0/0
    else if (total >= 0) status = 1;
    else status = -1;
    return true;
}

// ---- SHA-256 (FIPS 180-4), small and dependency-free ----------------------
struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t buf[64] = {};
    size_t buf_len = 0;
    uint64_t total = 0;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
                   (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* p, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - buf_len;
            if (take > n) take = n;
            std::memcpy(buf + buf_len, p, take);
            buf_len += take; p += take; n -= take;
            if (buf_len == 64) { block(buf); buf_len = 0; }
        }
    }

    std::string hex() {
        uint64_t bits = total * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (buf_len != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
        update(len, 8);
        char out[65];
        for (int i = 0; i < 8; ++i) std::snprintf(out + i * 8, 9, "%08x", h[i]);
        return std::string(out, 64);
    }
};

} // namespace

// ---------------------------------------------------------------------------

bool TestRunConfig::from_env(TestRunConfig& out) {
    const char* path = std::getenv("VELOCE_TEST_OUT");
    if (!path || !*path) return false;
    out.out_path = path;
    out.exit_on_finish = env_flag("VELOCE_TEST_EXIT", true);
    if (const char* r = std::getenv("VELOCE_TEST_RESETS")) {
        if (*r) {
            int v = std::atoi(r);
            out.max_resets = v < 0 ? 0 : v;
        }
    }
    return true;
}

TestFileSink::TestFileSink(TestRunConfig config) : m_config(std::move(config)) {}

TestFileSink::~TestFileSink() {
    // Never leave the plugin holding a dangling sink; close without a
    // trailer reason override if the owner forgot to.
    if (!m_closed) close(EndReason::Quit, m_frames, 0);
}

const char* TestFileSink::reason_name(EndReason reason) {
    switch (reason) {
        case EndReason::Terminator: return "terminator";
        case EndReason::Frames:     return "frames";
        case EndReason::ResetLimit: return "reset_limit";
        case EndReason::Quit:       return "quit";
    }
    return "quit";
}

std::string TestFileSink::sha256_hex(const uint8_t* data, size_t n) {
    Sha256 s;
    if (data && n) s.update(data, n);
    return s.hex();
}

bool TestFileSink::open() {
    if (m_file) return true;
    m_file = std::fopen(m_config.out_path.c_str(), "wb");
    return m_file != nullptr;
}

void TestFileSink::attach(IEmulatorPlugin* plugin, uint32_t plugin_api_version) {
    m_plugin = plugin;
    m_plugin_has_test_api = plugin && plugin_api_version >= EMU_PLUGIN_API_VERSION_TEST_SINK;

    std::string core = "unknown";
    std::string channels;
    uint32_t crc = 0;
    if (plugin) {
        EmulatorInfo info = plugin->get_info();
        if (info.name && *info.name) core = token_safe(info.name);
        crc = plugin->get_rom_crc32();
        if (m_plugin_has_test_api) channels = token_safe(plugin->test_channels());
    }
    char head[256];
    std::snprintf(head, sizeof(head), "#VELOCE 1 core=%s rom_crc32=%08" PRIx32 " channels=%s",
                  core.c_str(), crc, channels.c_str());
    emit_meta(head);

    if (m_plugin_has_test_api) {
        m_sink_accepted = plugin->set_test_sink(this);
    }
}

void TestFileSink::detach() {
    if (m_plugin && m_plugin_has_test_api && m_sink_accepted) {
        m_plugin->set_test_sink(nullptr);
    }
    m_plugin = nullptr;
    m_sink_accepted = false;
}

bool TestFileSink::after_frame(uint64_t frames_run) {
    m_frames = frames_run;
    if (m_reset_pending && !m_finished && frames_run >= m_reset_at) {
        m_reset_pending = false;
        if (m_plugin) {
            m_plugin->reset();
            ++m_resets_done;
            emit_meta("#VELOCE reset n=" + std::to_string(m_resets_done) +
                      " frame=" + std::to_string(frames_run));
            if (m_plugin_has_test_api) m_plugin->on_test_reset();
        }
    }
    if (m_reset_limit_hit) return true;
    return m_finished && m_config.exit_on_finish;
}

void TestFileSink::close(EndReason fallback, uint64_t frames_run, uint64_t cycles) {
    if (m_closed) return;
    detach();
    flush_partial();      // may complete an END line and call finish()
    m_closed = true;

    EndReason reason = fallback;
    if (m_reset_limit_hit) reason = EndReason::ResetLimit;
    else if (m_finished && m_config.exit_on_finish) reason = EndReason::Terminator;

    std::string trailer = std::string("#VELOCE end reason=") + reason_name(reason) +
                          " frames=" + std::to_string(frames_run) +
                          " cycles=" + std::to_string(cycles);
    if (m_finished) trailer += " status=" + std::to_string(m_status);
    if (m_resets_done) trailer += " resets=" + std::to_string(m_resets_done);
    emit_meta(trailer);

    if (m_file) {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

// ---- ITestSink ------------------------------------------------------------

void TestFileSink::write(const char* bytes, size_t n) {
    if (m_closed || !bytes) return;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c == '\n') {
            std::string line;
            line.swap(m_line);
            m_line_truncated = false;
            emit_line(line);
            continue;
        }
        if (c == '\r') continue;
        if ((c < 0x20 && c != '\t') || c == 0x7F) continue;   // control bytes
        if (c >= 0x80) c = '?';                                  // keep the file ASCII
        if (m_line.size() >= kMaxLineBytes) {
            m_line_truncated = true;
            continue;
        }
        m_line.push_back(static_cast<char>(c));
    }
}

void TestFileSink::finish(int32_t status_code) {
    if (m_closed || m_finished) return;
    m_finished = true;
    m_status = status_code;
    m_reset_pending = false;
}

void TestFileSink::note(const char* key, const char* value) {
    if (m_closed) return;
    emit_meta("#VELOCE " + key_safe(key) + "=" + value_safe(value));
}

void TestFileSink::blob(const char* name, const uint8_t* bytes, size_t n) {
    if (m_closed) return;
    std::string safe = key_safe(name);
    std::string path = m_config.out_path + "." + safe + ".bin";
    bool ok = false;
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        ok = (n == 0) || (bytes && std::fwrite(bytes, 1, n, f) == n);
        ok = (std::fclose(f) == 0) && ok;
    }
    std::string line = "#VELOCE blob name=" + safe + " bytes=" + std::to_string(n);
    line += ok ? " sha256=" + sha256_hex(bytes, n) : std::string(" error=write_failed");
    emit_meta(line);
}

void TestFileSink::request_reset() {
    if (m_closed || m_finished || m_reset_pending || m_reset_limit_hit) return;
    if (m_resets_done >= m_config.max_resets) {
        m_reset_limit_hit = true;
        emit_meta("#VELOCE reset_limit=" + std::to_string(m_config.max_resets));
        return;
    }
    m_reset_pending = true;
    m_reset_at = m_frames + static_cast<uint64_t>(m_config.reset_delay_frames);
}

// ---- output ---------------------------------------------------------------

void TestFileSink::emit_line(const std::string& raw) {
    // A ROM must not be able to forge the application's "#VELOCE" metadata:
    // channel lines starting with '#' are carried as LOG text.
    std::string line = (!raw.empty() && raw[0] == '#') ? "LOG " + raw : raw;
    if (line.size() > kMaxLineBytes) line.resize(kMaxLineBytes);
    if (m_file) {
        std::fwrite(line.data(), 1, line.size(), m_file);
        std::fputc('\n', m_file);
        std::fflush(m_file);
    }
    int32_t status = -1;
    if (parse_end_line(line, status)) finish(status);
}

void TestFileSink::emit_meta(const std::string& line) {
    if (!m_file) return;
    std::fwrite(line.data(), 1, line.size(), m_file);
    std::fputc('\n', m_file);
    std::fflush(m_file);
}

void TestFileSink::flush_partial() {
    if (m_line.empty()) return;
    std::string line;
    line.swap(m_line);
    m_line_truncated = false;
    emit_line(line);
}

} // namespace emu
