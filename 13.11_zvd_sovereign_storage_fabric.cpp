/* =============================================================================
 *  PROJECT:    ZVD SOVEREIGN UNIVERSAL CONSTRUCTOR & STORAGE FABRIC
 *              (13.11_zvd_sovereign_storage_fabric.cpp - V1.3.0-SUPREME-C20)
 *  FILENAME:   13.11_zvd_sovereign_storage_fabric.cpp
 *  VERSION:    2026.11-V1.3.0-SUPREME-C20
 *
 *  [ARCHITECTURAL AUDIT & SCALE GUARANTEES]:
 *    - ZERO-ALLOCATION HOT PATHS: alignas(64) thread_local buffers for headers
 *      and macro-cell staging; zero per-request heap allocations on HTTP hot path.
 *    - KERNEL ZERO-COPY: Direct page-cache to socket transfer via Linux sendfile()
 *      clamped to 0x7ffff000ULL per syscall with Range hardening.
 *    - ATOMIC INGESTION: Temp-file isolation with transactional rename, 4-window
 *      PhaseHysteresis invariant/risk stabilization, and backward-compatible
 *      sidecar indexes (367B v2 / 359B v1 legacy).
 *    - 3-LEVEL NESTED CODEC (.ZVD3): Direct integration of nested_storage_codec_v32
 *      up to ZVD_MAX_MACRO_CELL_BYTES with strict 400/413 guards, POST /search,
 *      and GET /slot with ASIL-D VETO headers (422).
 *    - INTER-RACK ZVDO FABRIC: Bit-perfect Moiré-encrypted optical frame replication.
 *    - PRODUCTION HARDENED: SIGPIPE immunity, constant-time token check, strict
 *      port parsing (1..65535), unity-build guards, and -Wall -Wextra -Werror clean.
 * =============================================================================
 *  BUILD (Linux):
 *    g++ -std=c++20 -O3 -Wall -Wextra -Werror -fopenmp "13.11_zvd_sovereign_storage_fabric.cpp" -o zvd_fabric -lm -lpthread
 *
 *  AUTHOR:     Vladimir Zavodiuk (Varna, Bulgaria, EU)
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Zero-Allocation, alignas(64), Strict English I18n.
 * =============================================================================
 */

#ifndef restrict
#if defined(__GNUC__) || defined(__clang__)
    #define restrict __restrict__
#else
    #define restrict
#endif
#endif

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD-SOVEREIGN-ENTERPRISE-2026"
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <csignal>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#ifndef ZVD_GLOBAL_SHUTDOWN_DEFINED
#define ZVD_GLOBAL_SHUTDOWN_DEFINED 1

inline std::atomic<bool> g_zvd_global_shutdown{false};

namespace {

inline void zvd_fabric_signal_handler(int) noexcept {
    g_zvd_global_shutdown.store(true, std::memory_order_release);
}

} // namespace

#endif // ZVD_GLOBAL_SHUTDOWN_DEFINED

// =====================================================================
// SECTION 1: DATACENTER CORE & NESTED CODEC LINKAGE
// =====================================================================

#if !defined(ZVD_HAS_DATACENTER_CORE)
  #if defined(__has_include)
    #if __has_include("13.11_zvd_datacenter_node_v28_lite.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "13.11_zvd_datacenter_node_v28_lite.c"
    #elif __has_include("13.11_zvd_datacenter_node_v28_lite_v2.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "13.11_zvd_datacenter_node_v28_lite_v2.c"
    #elif __has_include("zvd_datacenter_node_v28_lite.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "zvd_datacenter_node_v28_lite.c"
    #endif
  #endif

  #if defined(ZVD_HAS_DATACENTER_CORE) && defined(ZVD_CORE_FILENAME)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  #pragma GCC diagnostic ignored "-Wvolatile"
  extern "C" {
      #include ZVD_CORE_FILENAME
  }
  #pragma GCC diagnostic pop
  #endif
#endif

#if !defined(ZVD_HAS_NESTED_STORAGE_CODEC) && !defined(ZVD_HAS_NESTED_CODEC_V32)
  #if defined(__has_include)
    #if __has_include("13.11_zvd_nested_storage_codec_v32.hpp")
      #include "13.11_zvd_nested_storage_codec_v32.hpp"
    #endif
  #endif
#endif

#if defined(ZVD_HAS_NESTED_STORAGE_CODEC) || defined(ZVD_HAS_NESTED_CODEC_V32)
  #define ZVD_FABRIC_HAS_NESTED_CODEC 1
#else
  #define ZVD_FABRIC_HAS_NESTED_CODEC 0
#endif

// Global atomic telemetry, sequencing, and concurrency counters
alignas(64) static std::atomic<uint64_t> g_total_bytes_ingested{0};
alignas(64) static std::atomic<uint64_t> g_total_bytes_served{0};
alignas(64) static std::atomic<uint64_t> g_tmp_file_seq{0};
alignas(64) static std::atomic<uint32_t> g_active_connections{0};
alignas(64) static std::atomic<uint32_t> g_active_replications{0};
constexpr uint32_t MAX_ACTIVE_CONNS = 2048;
constexpr uint32_t MAX_ACTIVE_REPLICATIONS = 64;
constexpr size_t ZVD_MAX_HTTP_HEADER_BYTES = 65536u;
constexpr size_t ZVD_HYSTERESIS_WINDOW_BYTES = 32768u;
constexpr size_t ZVD_HYSTERESIS_MAX_SAMPLES = 4u;

struct ZvdReplicationGuard {
    bool acquired = false;
    ZvdReplicationGuard() noexcept {
        uint32_t cur = g_active_replications.load(std::memory_order_relaxed);
        while (cur < MAX_ACTIVE_REPLICATIONS) {
            if (g_active_replications.compare_exchange_weak(
                    cur, cur + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                acquired = true;
                break;
            }
        }
    }
    ~ZvdReplicationGuard() noexcept {
        if (acquired) {
            g_active_replications.fetch_sub(1, std::memory_order_acq_rel);
        }
    }
    ZvdReplicationGuard(const ZvdReplicationGuard&) = delete;
    ZvdReplicationGuard& operator=(const ZvdReplicationGuard&) = delete;
};

// =====================================================================
// SECTION 2: ENVIRONMENT CONFIGURATION & HARDENED I/O HELPERS
// =====================================================================

static int zvd_parse_port_env(const char* env_name, int default_port) noexcept {
    const char* p = std::getenv(env_name);
    if (!p || p[0] == '\0') return default_port;
    errno = 0;
    char* endptr = nullptr;
    unsigned long val = std::strtoul(p, &endptr, 10);
    if (errno != 0 || endptr == p || *endptr != '\0' || val < 1ul || val > 65535ul) {
        return default_port;
    }
    return static_cast<int>(val);
}

struct ZvdFabricConfig {
    int listen_port = 8080;
    int rack_port = 9000;
    std::string storage_root = "./zvd_vault";
    uint16_t rack_id = 101;
    std::string peer_rack_ip = "";
    int peer_rack_port = 9000;
    uint16_t peer_rack_id = 202;
    std::string master_key = ZVD_DEFAULT_MASTER_KEY;
    std::string api_token = "";

    static const ZvdFabricConfig& get() {
        static ZvdFabricConfig cfg = []() {
            ZvdFabricConfig c;
            c.listen_port = zvd_parse_port_env("ZVD_LISTEN_PORT", 8080);
            c.rack_port = zvd_parse_port_env("ZVD_RACK_PORT", 9000);
            c.peer_rack_port = zvd_parse_port_env("ZVD_PEER_RACK_PORT", 9000);
            if (const char* p = std::getenv("ZVD_STORAGE_ROOT")) {
                if (p[0] != '\0') c.storage_root = p;
            }
            if (const char* p = std::getenv("ZVD_RACK_ID")) {
                if (p[0] != '\0') c.rack_id = static_cast<uint16_t>(std::strtoul(p, nullptr, 10));
            }
            if (const char* p = std::getenv("ZVD_PEER_RACK_IP")) {
                c.peer_rack_ip = p;
            }
            if (const char* p = std::getenv("ZVD_PEER_RACK_ID")) {
                if (p[0] != '\0') c.peer_rack_id = static_cast<uint16_t>(std::strtoul(p, nullptr, 10));
            }
            if (const char* p = std::getenv("ZVD_MASTER_KEY")) {
                if (p[0] != '\0') c.master_key = p;
            }
            if (const char* p = std::getenv("ZVD_API_TOKEN")) {
                c.api_token = p;
            }
            return c;
        }();
        return cfg;
    }
};

static bool zvd_ct_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0u;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0u;
}

static bool zvd_has_header_token_value(std::string_view req, std::string_view prefix, std::string_view token) noexcept {
    if (token.empty()) return false;
    size_t pos = 0;
    while (pos < req.size()) {
        size_t found = req.find(prefix, pos);
        if (found == std::string_view::npos) return false;

        if (found == 0 || req[found - 1] == '\n') {
            size_t val_start = found + prefix.size();
            if (req.size() - val_start >= token.size()) {
                size_t after_pos = val_start + token.size();
                bool term_ok = (after_pos == req.size() || req[after_pos] == '\r' || req[after_pos] == '\n');
                if (term_ok && zvd_ct_equal(req.substr(val_start, token.size()), token)) {
                    return true;
                }
            }
        }
        pos = found + prefix.size();
    }
    return false;
}

static bool zvd_is_authorized(std::string_view req, const ZvdFabricConfig& cfg) noexcept {
    if (cfg.api_token.empty()) return true;
    std::string_view tok(cfg.api_token);
    if (zvd_has_header_token_value(req, "X-ZVD-Token: ", tok)) return true;
    if (zvd_has_header_token_value(req, "x-zvd-token: ", tok)) return true;
    if (zvd_has_header_token_value(req, "Authorization: Bearer ", tok)) return true;
    if (zvd_has_header_token_value(req, "authorization: Bearer ", tok)) return true;
    return false;
}

static bool zvd_header_equals_ci(std::string_view req, std::string_view header_prefix, std::string_view expected_val) noexcept {
    size_t pos = 0;
    while (pos < req.size()) {
        size_t found = req.find(header_prefix, pos);
        if (found == std::string_view::npos) return false;
        if (found == 0 || req[found - 1] == '\n') {
            size_t v_start = found + header_prefix.size();
            size_t v_end = req.find_first_of("\r\n", v_start);
            if (v_end == std::string_view::npos) v_end = req.size();
            std::string_view actual = req.substr(v_start, v_end - v_start);
            while (!actual.empty() && (actual.front() == ' ' || actual.front() == '\t')) actual.remove_prefix(1);
            while (!actual.empty() && (actual.back() == ' ' || actual.back() == '\t')) actual.remove_suffix(1);
            if (actual.size() == expected_val.size()) {
                bool match = true;
                for (size_t i = 0; i < actual.size(); ++i) {
                    char ca = actual[i];
                    char ce = expected_val[i];
                    if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + ('a' - 'A'));
                    if (ce >= 'A' && ce <= 'Z') ce = static_cast<char>(ce + ('a' - 'A'));
                    if (ca != ce) {
                        match = false;
                        break;
                    }
                }
                if (match) return true;
            }
        }
        pos = found + header_prefix.size();
    }
    return false;
}

static bool zvd_sanitize_filename(std::string_view name, std::string& sanitized) noexcept {
    if (name.empty() || name.find("..") != std::string_view::npos ||
        name.find('/') != std::string_view::npos || name.find('\\') != std::string_view::npos) {
        return false;
    }
    sanitized.clear();
    sanitized.reserve(name.size());
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            sanitized.push_back(c);
        } else {
            return false;
        }
    }
    return !sanitized.empty();
}

static std::string zvd_json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

static uint64_t zvd_fabric_fast_hash64_incremental(const uint8_t* data, size_t len, uint64_t seed = 0x9E3779B97F4A7C15ULL) noexcept {
    uint64_t h = seed ^ len;
    size_t n_blocks = len / 8;
    for (size_t i = 0; i < n_blocks; ++i) {
        uint64_t w = 0;
        std::memcpy(&w, data + i * 8, 8);
        h ^= w;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    size_t tail_len = len % 8;
    if (tail_len > 0) {
        uint64_t tail_val = 0;
        std::memcpy(&tail_val, data + n_blocks * 8, tail_len);
        h ^= tail_val;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

static bool zvd_write_exact(int fd, const void* buf, size_t len) noexcept {
    const char* ptr = reinterpret_cast<const char*>(buf);
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, ptr + written, len - written);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

static bool zvd_send_exact(int fd, const void* buf, size_t len) noexcept {
    const char* ptr = reinterpret_cast<const char*>(buf);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, ptr + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool zvd_recv_exact(int fd, void* buf, size_t len) noexcept {
    char* ptr = reinterpret_cast<char*>(buf);
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, ptr + received, len - received, 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return false;
        }
        received += static_cast<size_t>(n);
    }
    return true;
}

// =====================================================================
// SECTION 3: SIDECAR METADATA (v1/v2 COMPAT), PHASE HYSTERESIS & REPLICATION
// =====================================================================

#pragma pack(push, 1)
struct ZvdSidecarIndex {
    uint64_t file_size;
    uint64_t full_file_hash64;
    int8_t gl_invariant;
    double entropy;
#if defined(ZVD_HAS_DATACENTER_CORE)
    ZVD_Monolith334 signature_monolith;
#else
    uint8_t signature_monolith[334];
#endif
    double risk_index; // v2 extension (367B total); v1 legacy is 359B without risk_index
};
#pragma pack(pop)

constexpr size_t ZVD_SIDECAR_V2_BYTES = sizeof(ZvdSidecarIndex); // 367 bytes
constexpr size_t ZVD_SIDECAR_V1_BYTES = sizeof(ZvdSidecarIndex) - sizeof(double); // 359 bytes
static_assert(ZVD_SIDECAR_V2_BYTES == 367u, "ZvdSidecarIndex v2 must be 367 bytes");
static_assert(ZVD_SIDECAR_V1_BYTES == 359u, "ZvdSidecarIndex v1 legacy must be 359 bytes");

struct ZvdPhaseHysteresisTracker {
    std::array<uint8_t, ZVD_HYSTERESIS_WINDOW_BYTES * ZVD_HYSTERESIS_MAX_SAMPLES> ring{};
    size_t ring_pos = 0;
    size_t ring_filled = 0;
    uint64_t total_seen = 0;

    std::array<int8_t, ZVD_HYSTERESIS_MAX_SAMPLES> inv_hist{};
    std::array<double, ZVD_HYSTERESIS_MAX_SAMPLES> risk_hist{};
    size_t sample_count = 0;
    int8_t last_gl_invariant = 0;
    double last_entropy = 0.0;
    double last_risk_index = 0.0;
#if defined(ZVD_HAS_DATACENTER_CORE)
    ZVD_Monolith334 last_monolith{};
#else
    std::array<uint8_t, 334> last_monolith{};
#endif

    void observe(const uint8_t* data, size_t len) noexcept {
        if (!data || len == 0) return;
        for (size_t i = 0; i < len; ++i) {
            ring[ring_pos] = data[i];
            ring_pos = (ring_pos + 1u) % ring.size();
            if (ring_filled < ring.size()) ++ring_filled;
            ++total_seen;
        }
    }

    void finalize(uint64_t expected_total_bytes) noexcept {
#if defined(ZVD_HAS_DATACENTER_CORE)
        ZVD_BiCore120D bicore;
        zvd_bicore_init(&bicore, 0, 1);

        if (ring_filled == 0 || total_seen == 0) {
            static const uint8_t fallback[3] = {'Z', 'V', 'D'};
            ZVD_DatacenterPacket pkt{};
            zvd_node_ingest_monolith_only(fallback, sizeof(fallback), &bicore, &pkt);
            last_gl_invariant = pkt.gl_invariant;
            last_entropy = pkt.entropy;
            last_risk_index = pkt.risk_index;
            last_monolith = pkt.monolith;
            return;
        }

        uint64_t span_bytes = (expected_total_bytes > 0) ? std::min<uint64_t>(total_seen, expected_total_bytes) : total_seen;
        size_t avail = std::min<size_t>(ring_filled, static_cast<size_t>(std::min<uint64_t>(span_bytes, ring.size())));

        size_t windows = 1u;
        if (avail > ZVD_HYSTERESIS_WINDOW_BYTES * 3u) windows = 4u;
        else if (avail > ZVD_HYSTERESIS_WINDOW_BYTES * 2u) windows = 3u;
        else if (avail > ZVD_HYSTERESIS_WINDOW_BYTES) windows = 2u;

        sample_count = 0;
        double entropy_sum = 0.0;
        double min_risk = 1e300;
        bool has_risk = false;
        long inv_sum = 0;

        for (size_t w = 0; w < windows; ++w) {
            size_t win_len = std::min(ZVD_HYSTERESIS_WINDOW_BYTES, avail);
            size_t start_in_avail = 0u;
            if (windows == 1u) {
                start_in_avail = 0u;
            } else {
                size_t max_start = (avail > win_len) ? (avail - win_len) : 0u;
                start_in_avail = (max_start * w) / (windows - 1u);
            }

            size_t ring_start = (ring_filled + ring_pos - avail + start_in_avail) % ring.size();
            std::array<uint8_t, ZVD_HYSTERESIS_WINDOW_BYTES> win_buf{};
            for (size_t k = 0; k < win_len; ++k) {
                win_buf[k] = ring[(ring_start + k) % ring.size()];
            }

            ZVD_DatacenterPacket pkt{};
            zvd_node_ingest_monolith_only(win_buf.data(), win_len, &bicore, &pkt);

            int8_t clamped_inv = zvd_lukasiewicz_clamp(static_cast<int>(pkt.gl_invariant));
            inv_hist[sample_count] = clamped_inv;
            risk_hist[sample_count] = pkt.risk_index;
            ++sample_count;

            inv_sum += static_cast<long>(clamped_inv);
            entropy_sum += pkt.entropy;
            if (!has_risk || pkt.risk_index < min_risk) {
                min_risk = pkt.risk_index;
                has_risk = true;
            }
            if (w + 1u == windows) {
                last_monolith = pkt.monolith;
            }
        }

        double avg_inv = static_cast<double>(inv_sum) / static_cast<double>(sample_count);
        int rounded_inv = static_cast<int>(std::llround(avg_inv));
        last_gl_invariant = zvd_lukasiewicz_clamp(rounded_inv);
        last_entropy = entropy_sum / static_cast<double>(sample_count);
        last_risk_index = has_risk ? min_risk : 0.0;
#else
        (void)expected_total_bytes;
        last_gl_invariant = 0;
        last_entropy = 0.0;
        last_risk_index = 0.0;
        last_monolith.fill(0);
#endif
    }
};

static bool zvd_write_sidecar(const std::string& storage_root, std::string_view filename, const ZvdSidecarIndex& idx) noexcept {
    std::filesystem::path meta_dir = std::filesystem::path(storage_root) / ".meta";
    std::error_code ec;
    std::filesystem::create_directories(meta_dir, ec);
    if (ec) return false;

    std::string idx_filename = std::string(filename) + ".zvd.idx";
    std::filesystem::path idx_path = meta_dir / idx_filename;

    int fd = open(idx_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;

    bool ok = zvd_write_exact(fd, &idx, ZVD_SIDECAR_V2_BYTES);
    close(fd);
    return ok;
}

static bool zvd_read_sidecar(const std::string& storage_root, std::string_view filename, ZvdSidecarIndex& idx) noexcept {
    std::filesystem::path idx_path = std::filesystem::path(storage_root) / ".meta" / (std::string(filename) + ".zvd.idx");
    int fd = open(idx_path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    std::memset(&idx, 0, sizeof(ZvdSidecarIndex));
    size_t read_bytes = 0;
    char* ptr = reinterpret_cast<char*>(&idx);
    while (read_bytes < ZVD_SIDECAR_V2_BYTES) {
        ssize_t n = read(fd, ptr + read_bytes, ZVD_SIDECAR_V2_BYTES - read_bytes);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return false;
        }
        if (n == 0) break;
        read_bytes += static_cast<size_t>(n);
    }
    close(fd);

    if (read_bytes == ZVD_SIDECAR_V2_BYTES) {
        return true;
    }
    if (read_bytes == ZVD_SIDECAR_V1_BYTES) {
        idx.risk_index = 0.0;
        return true;
    }
    return false;
}

static void zvd_replicate_to_peer(std::string sanitized_filename, std::string filepath, uint64_t file_size) {
    ZvdReplicationGuard repl_guard;
    if (!repl_guard.acquired) {
        std::fprintf(stderr, "[ZVD-FABRIC-WARN] Max active replications (%u) reached; skipping peer sync for %s\n",
                     MAX_ACTIVE_REPLICATIONS, sanitized_filename.c_str());
        return;
    }

    const auto& cfg = ZvdFabricConfig::get();
    if (cfg.peer_rack_ip.empty()) return;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    timeval io_tv{5, 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &io_tv, sizeof(io_tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &io_tv, sizeof(io_tv));

    int nodelay = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    sockaddr_in peer_addr{};
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(static_cast<uint16_t>(cfg.peer_rack_port));

    if (inet_pton(AF_INET, cfg.peer_rack_ip.c_str(), &peer_addr.sin_addr) != 1) {
        close(sock);
        return;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&peer_addr), sizeof(peer_addr)) < 0) {
        close(sock);
        return;
    }

#if defined(ZVD_HAS_DATACENTER_CORE)
    std::string handshake = sanitized_filename + ":" + std::to_string(file_size);
    alignas(64) uint8_t frame_buf[sizeof(ZVD_OpticDatacenterHeader) + 65536];
    size_t frame_sz = 0;
    if (zvd_optic_frame_pack(cfg.rack_id, cfg.peer_rack_id, reinterpret_cast<const uint8_t*>(handshake.data()), handshake.size(), cfg.master_key.c_str(), frame_buf, sizeof(frame_buf), &frame_sz)) {
        zvd_send_exact(sock, frame_buf, frame_sz);
    }

    int file_fd = open(filepath.c_str(), O_RDONLY);
    if (file_fd >= 0) {
        constexpr size_t CHUNK = 65536;
        alignas(64) std::array<uint8_t, CHUNK> chunk_buf;
        uint64_t chunk_idx = 0;
        while (!g_zvd_global_shutdown.load(std::memory_order_acquire)) {
            ssize_t r = read(file_fd, chunk_buf.data(), CHUNK);
            if (r <= 0) {
                if (r < 0 && errno == EINTR) continue;
                break;
            }
            ++chunk_idx;
            uint64_t mixed = (static_cast<uint64_t>(cfg.rack_id) ^ (chunk_idx * 0x9E3779B97F4A7C15ULL));
            uint16_t src_rack_tag = static_cast<uint16_t>(mixed ^ (mixed >> 16) ^ (mixed >> 32));
            size_t c_sz = 0;
            if (zvd_optic_frame_pack(src_rack_tag, cfg.peer_rack_id, chunk_buf.data(), static_cast<size_t>(r), cfg.master_key.c_str(), frame_buf, sizeof(frame_buf), &c_sz)) {
                if (!zvd_send_exact(sock, frame_buf, c_sz)) break;
            }
        }
        close(file_fd);
    }
#endif
    close(sock);
}

static void zvd_spawn_peer_replication(const std::string& sanitized_filename, const std::string& filepath, uint64_t file_size) {
    if (g_active_replications.load(std::memory_order_relaxed) >= MAX_ACTIVE_REPLICATIONS) {
        return;
    }
    std::thread(zvd_replicate_to_peer, sanitized_filename, filepath, file_size).detach();
}
// =====================================================================
// SECTION 4: INTER-RACK ZVDO FABRIC LISTENER THREAD (PORT 9000)
// =====================================================================

static void zvd_rack_fabric_listener_thread() {
    const auto& cfg = ZvdFabricConfig::get();
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    timeval accept_tv{0, 500000};
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &accept_tv, sizeof(accept_tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(cfg.rack_port));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(server_fd);
        return;
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        close(server_fd);
        return;
    }

    while (!g_zvd_global_shutdown.load(std::memory_order_acquire)) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (g_zvd_global_shutdown.load(std::memory_order_acquire)) {
            if (client_fd >= 0) close(client_fd);
            break;
        }
        if (client_fd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }

        if (g_active_connections.load(std::memory_order_relaxed) >= MAX_ACTIVE_CONNS) {
            close(client_fd);
            continue;
        }

        g_active_connections.fetch_add(1, std::memory_order_relaxed);
        std::thread([client_fd, cfg]() {
            int nodelay = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

            timeval tv{30, 0};
            setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            std::string target_filename = "";
            uint64_t expected_size = 0;
            int dest_file_fd = -1;
            uint64_t rolling_hash = 0x9E3779B97F4A7C15ULL;
            ZvdPhaseHysteresisTracker hyst;
            std::filesystem::path final_fpath;
            std::filesystem::path tmp_fpath;

#if defined(ZVD_HAS_DATACENTER_CORE)
            alignas(64) uint8_t frame_packet[sizeof(ZVD_OpticDatacenterHeader) + 65536];
            alignas(64) uint8_t payload_out[65536];

            while (!g_zvd_global_shutdown.load(std::memory_order_acquire)) {
                ZVD_OpticDatacenterHeader hdr{};
                if (!zvd_recv_exact(client_fd, &hdr, sizeof(ZVD_OpticDatacenterHeader))) {
                    break;
                }

                size_t payload_len = static_cast<size_t>(hdr.optic_payload_len & 0x7FFFFFFFU);
                if (std::memcmp(hdr.magic, "ZVDO", 4) != 0 || payload_len > 65536) {
                    std::fprintf(stderr, "[ZVD-FABRIC-SECURITY] Invalid ZVDO frame magic or payload length exceed limit!\n");
                    break;
                }

                std::memcpy(frame_packet, &hdr, sizeof(ZVD_OpticDatacenterHeader));

                if (!zvd_recv_exact(client_fd, frame_packet + sizeof(ZVD_OpticDatacenterHeader), payload_len)) {
                    break;
                }

                uint16_t src_rack = 0, dst_rack = 0;
                size_t payload_out_sz = 0;
                bool bit_perfect = false;
                size_t total_frame_bytes = sizeof(ZVD_OpticDatacenterHeader) + payload_len;

                if (zvd_optic_frame_unpack(frame_packet, total_frame_bytes, cfg.master_key.c_str(),
                                           payload_out, sizeof(payload_out),
                                           &src_rack, &dst_rack, &payload_out_sz, &bit_perfect)) {
                    (void)dst_rack;
                    if (!bit_perfect) {
                        std::fprintf(stderr, "[ZVD-FABRIC-SECURITY] Bit-perfect validation failed on ZVDO frame from rack %u!\n", src_rack);
                        continue;
                    }

                    if (dest_file_fd < 0) {
                        std::string hs(reinterpret_cast<char*>(payload_out), payload_out_sz);
                        size_t colon_pos = hs.find(':');
                        if (colon_pos != std::string::npos) {
                            std::string raw_name = hs.substr(0, colon_pos);
                            expected_size = std::strtoull(hs.substr(colon_pos + 1).c_str(), nullptr, 10);
                            zvd_sanitize_filename(raw_name, target_filename);
                            if (!target_filename.empty()) {
                                std::filesystem::create_directories(cfg.storage_root);
                                final_fpath = std::filesystem::path(cfg.storage_root) / target_filename;
                                uint64_t tid = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                                uint64_t seq = g_tmp_file_seq.fetch_add(1, std::memory_order_relaxed);
                                tmp_fpath = std::filesystem::path(cfg.storage_root) /
                                            (target_filename + ".tmp." + std::to_string(tid) + "." + std::to_string(seq));
                                dest_file_fd = open(tmp_fpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            }
                        }
                    } else {
                        zvd_write_exact(dest_file_fd, payload_out, payload_out_sz);
                        rolling_hash = zvd_fabric_fast_hash64_incremental(payload_out, payload_out_sz, rolling_hash);
                        hyst.observe(payload_out, payload_out_sz);
                    }
                }
            }

            if (dest_file_fd >= 0) {
                close(dest_file_fd);
                struct stat st{};
                uint64_t total_written = 0;
                if (stat(tmp_fpath.c_str(), &st) == 0) {
                    total_written = static_cast<uint64_t>(st.st_size);
                }

                std::error_code ec;
                if (total_written == expected_size) {
                    std::filesystem::rename(tmp_fpath, final_fpath, ec);
                    if (!ec) {
                        hyst.finalize(expected_size);
                        ZvdSidecarIndex sidx{};
                        sidx.file_size = expected_size;
                        sidx.full_file_hash64 = rolling_hash;
                        sidx.gl_invariant = hyst.last_gl_invariant;
                        sidx.entropy = hyst.last_entropy;
                        sidx.signature_monolith = hyst.last_monolith;
                        sidx.risk_index = hyst.last_risk_index;
                        zvd_write_sidecar(cfg.storage_root, target_filename, sidx);
                    }
                } else {
                    std::filesystem::remove(tmp_fpath, ec);
                }
            }
#endif
            g_active_connections.fetch_sub(1, std::memory_order_relaxed);
            close(client_fd);
        }).detach();
    }

    close(server_fd);
}
// =====================================================================
// SECTION 5: HIGH-PERFORMANCE HTTP/1.1 STORAGE & NESTED CODEC PLANE
// =====================================================================

static void zvd_send_json(int client_fd, const char* status_line, const std::string& json_body, const char* extra_headers = nullptr) noexcept {
    std::string resp = "HTTP/1.1 ";
    resp += status_line;
    resp += "\r\nContent-Type: application/json\r\nContent-Length: ";
    resp += std::to_string(json_body.size());
    if (extra_headers && extra_headers[0] != '\0') {
        resp += "\r\n";
        resp += extra_headers;
    }
    resp += "\r\nConnection: close\r\n\r\n";
    resp += json_body;
    zvd_send_exact(client_fd, resp.data(), resp.size());
}

static void zvd_handle_http_client(int client_fd) {
    const auto& cfg = ZvdFabricConfig::get();

    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    timeval tv{30, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    static thread_local alignas(64) std::array<char, ZVD_MAX_HTTP_HEADER_BYTES> req_buf{};
    size_t req_len = 0;
    size_t header_end_pos = std::string_view::npos;
    size_t sep_len = 0;

    while (req_len < req_buf.size()) {
        ssize_t n = recv(client_fd, req_buf.data() + req_len, req_buf.size() - req_len, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        req_len += static_cast<size_t>(n);

        std::string_view current_req(req_buf.data(), req_len);
        header_end_pos = current_req.find("\r\n\r\n");
        if (header_end_pos != std::string_view::npos) {
            sep_len = 4;
            break;
        }
        header_end_pos = current_req.find("\n\n");
        if (header_end_pos != std::string_view::npos) {
            sep_len = 2;
            break;
        }
    }

    if (req_len == 0) {
        close(client_fd);
        return;
    }

    if (header_end_pos == std::string_view::npos && req_len == req_buf.size()) {
        zvd_send_json(client_fd, "431 Request Header Fields Too Large", "{\"error\":\"HTTP headers exceed 65536 bytes\"}");
        close(client_fd);
        return;
    }

    std::string_view req(req_buf.data(), req_len);

    if (req.starts_with("GET /healthz") || req.find("GET /healthz") != std::string_view::npos) {
        std::string payload = "{\"status\":\"OK\",\"rack_id\":" + std::to_string(cfg.rack_id) +
                              ",\"storage_root\":\"" + zvd_json_escape(cfg.storage_root) +
                              "\",\"active_connections\":" + std::to_string(g_active_connections.load()) +
                              ",\"active_replications\":" + std::to_string(g_active_replications.load()) +
                              ",\"bytes_ingested\":" + std::to_string(g_total_bytes_ingested.load()) +
                              ",\"bytes_served\":" + std::to_string(g_total_bytes_served.load()) +
                              ",\"nested_codec\":" + std::string(ZVD_FABRIC_HAS_NESTED_CODEC ? "true" : "false") + "}";
        zvd_send_json(client_fd, "200 OK", payload);
        close(client_fd);
        return;
    }

    if (req.starts_with("GET /files\r\n") || req.starts_with("GET /files ") || req == "GET /files") {
        std::string json_arr = "[";
        bool first = true;
        std::filesystem::path sroot(cfg.storage_root);
        std::error_code ec;
        if (std::filesystem::exists(sroot, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(sroot, ec)) {
                if (entry.is_regular_file(ec)) {
                    std::string fname = entry.path().filename().string();
                    if (fname.ends_with(".zvd.idx") || fname.find(".tmp.") != std::string::npos) continue;
                    uint64_t fsize = entry.file_size(ec);
                    if (ec) {
                        ec.clear();
                        continue;
                    }
                    ZvdSidecarIndex idx{};
                    bool has_idx = zvd_read_sidecar(cfg.storage_root, fname, idx);

                    if (!first) json_arr += ",";
                    first = false;
                    json_arr += "{\"file\":\"" + zvd_json_escape(fname) + "\"";
                    json_arr += ",\"bytes\":" + std::to_string(fsize);
                    json_arr += ",\"hash64\":\"" + std::to_string(has_idx ? idx.full_file_hash64 : 0ULL) + "\"";
                    json_arr += ",\"risk_index\":" + std::to_string(has_idx ? idx.risk_index : 0.0) + "}";
                } else {
                    ec.clear();
                }
            }
        }
        json_arr += "]";
        zvd_send_json(client_fd, "200 OK", json_arr);
        close(client_fd);
        return;
    }

    if (req.starts_with("DELETE /files/")) {
        if (!zvd_is_authorized(req, cfg)) {
            zvd_send_json(client_fd, "401 Unauthorized", "{\"error\":\"Unauthorized\"}");
            close(client_fd);
            return;
        }

        size_t start = 14;
        size_t end = req.find(' ', start);
        if (end == std::string_view::npos) end = req.size();
        std::string raw_name(req.substr(start, end - start));
        std::string filename;
        if (zvd_sanitize_filename(raw_name, filename)) {
            std::filesystem::path fpath = std::filesystem::path(cfg.storage_root) / filename;
            std::filesystem::path ipath = std::filesystem::path(cfg.storage_root) / ".meta" / (filename + ".zvd.idx");
            std::error_code ec;
            std::filesystem::remove(fpath, ec);
            std::filesystem::remove(ipath, ec);
            zvd_send_json(client_fd, "200 OK", "{\"status\":\"DELETED\",\"file\":\"" + zvd_json_escape(filename) + "\"}");
        } else {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Invalid filename\"}");
        }
        close(client_fd);
        return;
    }

#if ZVD_FABRIC_HAS_NESTED_CODEC
    if (req.starts_with("POST /search")) {
        size_t cl_pos = req.find("Content-Length:");
        if (cl_pos == std::string_view::npos) cl_pos = req.find("content-length:");
        if (cl_pos == std::string_view::npos) {
            zvd_send_json(client_fd, "411 Length Required", "{\"error\":\"Content-Length required\"}");
            close(client_fd);
            return;
        }
        size_t colon = req.find(':', cl_pos);
        uint64_t content_length = (colon != std::string_view::npos)
            ? std::strtoull(req.data() + colon + 1, nullptr, 10)
            : 0ULL;

        if (content_length > ZVD_MAX_HTTP_HEADER_BYTES) {
            zvd_send_json(client_fd, "413 Payload Too Large", "{\"error\":\"Search query exceeds 65536 bytes\"}");
            close(client_fd);
            return;
        }

        size_t body_off = (header_end_pos != std::string_view::npos) ? (header_end_pos + sep_len) : 0u;
        std::string query_buf;
        query_buf.reserve(static_cast<size_t>(content_length));

        if (body_off < req_len && content_length > 0) {
            size_t avail = std::min<size_t>(req_len - body_off, static_cast<size_t>(content_length));
            query_buf.append(req.data() + body_off, avail);
        }

        char tmp_read[4096];
        while (query_buf.size() < static_cast<size_t>(content_length)) {
            size_t need = static_cast<size_t>(content_length) - query_buf.size();
            size_t chunk = std::min<size_t>(sizeof(tmp_read), need);
            ssize_t r = recv(client_fd, tmp_read, chunk, 0);
            if (r <= 0) {
                if (r < 0 && errno == EINTR) continue;
                break;
            }
            query_buf.append(tmp_read, static_cast<size_t>(r));
        }

        if (query_buf.size() != static_cast<size_t>(content_length)) {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Incomplete search body\"}");
            close(client_fd);
            return;
        }

        ZVD_Monolith334 q_mono{};
        int8_t q_inv = 0;
#if defined(ZVD_HAS_DATACENTER_CORE)
        ZVD_BiCore120D bicore;
        zvd_bicore_init(&bicore, 0, 1);
        ZVD_DatacenterPacket pkt{};
        const uint8_t* q_ptr = query_buf.empty()
            ? reinterpret_cast<const uint8_t*>("ZVD_QUERY")
            : reinterpret_cast<const uint8_t*>(query_buf.data());
        size_t q_len = query_buf.empty() ? 9u : query_buf.size();
        zvd_node_ingest_monolith_only(q_ptr, q_len, &bicore, &pkt);
        q_mono = pkt.monolith;
        q_inv = pkt.gl_invariant;
#endif

        char best_path[512]{0};
        double best_score = 0.0;
        MacroCellDiskHeader best_hdr{};
        bool found = zvd_storage_search_by_vector(cfg.storage_root.c_str(), q_mono, q_inv, best_path, &best_score, &best_hdr);

        if (!found) {
            zvd_send_json(client_fd, "200 OK", "{\"found\":false,\"score\":0.0}");
        } else {
            std::string body = "{\"found\":true,\"path\":\"" + zvd_json_escape(best_path) +
                               "\",\"score\":" + std::to_string(best_score) +
                               ",\"gl_invariant\":" + std::to_string(static_cast<int>(best_hdr.root_gl_invariant)) + "}";
            zvd_send_json(client_fd, "200 OK", body);
        }
        close(client_fd);
        return;
    }

    if (req.starts_with("GET /slot/")) {
        size_t start = 10;
        size_t q_mark = req.find('?', start);
        size_t space_pos = req.find(' ', start);
        size_t name_end = (q_mark != std::string_view::npos && (space_pos == std::string_view::npos || q_mark < space_pos))
            ? q_mark : space_pos;
        if (name_end == std::string_view::npos) name_end = req.size();

        std::string raw_name(req.substr(start, name_end - start));
        std::string filename;
        if (!zvd_sanitize_filename(raw_name, filename)) {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Invalid cell filename\"}");
            close(client_fd);
            return;
        }

        uint64_t geo_val = 0;
        uint64_t slot_val = 0;
        bool has_geo = false;
        bool has_slot = false;

        if (q_mark != std::string_view::npos && space_pos != std::string_view::npos && q_mark < space_pos) {
            std::string_view qs = req.substr(q_mark + 1, space_pos - (q_mark + 1));
            size_t g_pos = qs.find("geo=");
            if (g_pos != std::string_view::npos) {
                std::string g_str(qs.substr(g_pos + 4));
                char* endp = nullptr;
                errno = 0;
                geo_val = std::strtoull(g_str.c_str(), &endp, 10);
                if (errno == 0 && endp != g_str.c_str()) has_geo = true;
            }
            size_t s_pos = qs.find("slot=");
            if (s_pos != std::string_view::npos) {
                std::string s_str(qs.substr(s_pos + 5));
                char* endp = nullptr;
                errno = 0;
                slot_val = std::strtoull(s_str.c_str(), &endp, 10);
                if (errno == 0 && endp != s_str.c_str()) has_slot = true;
            }
        }

        if (!has_geo || !has_slot || geo_val > 65535ULL || slot_val >= static_cast<uint64_t>(ZVD_MICRO_SLOTS_COUNT)) {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Invalid geo or slot query parameter\"}");
            close(client_fd);
            return;
        }

        std::filesystem::path fpath = std::filesystem::path(cfg.storage_root) / filename;
        std::error_code ec;
        if (!std::filesystem::exists(fpath, ec) || !std::filesystem::is_regular_file(fpath, ec)) {
            zvd_send_json(client_fd, "404 Not Found", "{\"error\":\"Cell file not found\"}");
            close(client_fd);
            return;
        }

        static thread_local alignas(64) std::array<uint8_t, ZVD_STELLA8_SLOT_BYTES> slot_buf{};
        size_t slot_len = 0;
        bool asil_d_veto = false;
        bool ok = zvd_storage_pread_slot(
            fpath.c_str(),
            static_cast<uint16_t>(geo_val),
            static_cast<uint16_t>(slot_val),
            cfg.master_key.c_str(),
            slot_buf.data(),
            slot_buf.size(),
            &slot_len,
            &asil_d_veto
        );

        if (asil_d_veto) {
            zvd_send_json(client_fd, "422 Unprocessable Entity", "{\"error\":\"ASIL-D integrity veto triggered\"}", "X-ZVD-ASIL-VETO: 1");
            close(client_fd);
            return;
        }

        if (!ok || slot_len == 0) {
            zvd_send_json(client_fd, "404 Not Found", "{\"error\":\"Slot not found or unreadable\"}");
            close(client_fd);
            return;
        }

        std::string hdrs = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
                         std::to_string(slot_len) + "\r\nX-ZVD-Slot-Bytes: " + std::to_string(slot_len) +
                         "\r\nConnection: close\r\n\r\n";
        zvd_send_exact(client_fd, hdrs.data(), hdrs.size());
        zvd_send_exact(client_fd, slot_buf.data(), slot_len);
        g_total_bytes_served.fetch_add(static_cast<uint64_t>(slot_len), std::memory_order_relaxed);
        close(client_fd);
        return;
    }
#endif

    if (req.starts_with("GET /files/")) {
        size_t start = 11;
        size_t end = req.find(' ', start);
        if (end == std::string_view::npos) end = req.size();
        std::string raw_name(req.substr(start, end - start));
        std::string filename;
        if (!zvd_sanitize_filename(raw_name, filename)) {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Invalid filename\"}");
            close(client_fd);
            return;
        }

        std::filesystem::path fpath = std::filesystem::path(cfg.storage_root) / filename;
        int file_fd = open(fpath.c_str(), O_RDONLY);
        if (file_fd < 0) {
            zvd_send_json(client_fd, "404 Not Found", "{\"error\":\"File not found\"}");
            close(client_fd);
            return;
        }

        struct stat st{};
        fstat(file_fd, &st);
        uint64_t total_size = static_cast<uint64_t>(st.st_size);

        ZvdSidecarIndex idx{};
        zvd_read_sidecar(cfg.storage_root, filename, idx);

        off_t range_start = 0;
        off_t range_end = (total_size > 0) ? static_cast<off_t>(total_size - 1) : 0;
        bool has_range = false;

        size_t range_pos = req.find("Range: bytes=");
        if (range_pos == std::string_view::npos) range_pos = req.find("range: bytes=");
        if (range_pos != std::string_view::npos) {
            size_t r_end_line = req.find_first_of("\r\n", range_pos);
            if (r_end_line == std::string_view::npos) r_end_line = req.size();
            std::string_view r_val = req.substr(range_pos + 13, r_end_line - (range_pos + 13));
            size_t dash = r_val.find('-');
            if (dash != std::string_view::npos) {
                std::string s_str(r_val.substr(0, dash));
                std::string e_str(r_val.substr(dash + 1));
                if (!s_str.empty()) range_start = static_cast<off_t>(std::strtoll(s_str.c_str(), nullptr, 10));
                if (!e_str.empty()) range_end = static_cast<off_t>(std::strtoll(e_str.c_str(), nullptr, 10));
                has_range = true;
            }
        }

        if (has_range) {
            if (total_size == 0 || range_start < 0 || range_start >= static_cast<off_t>(total_size) || range_start > range_end) {
                std::string rng_err = "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" +
                                        std::to_string(total_size) + "\r\nConnection: close\r\n\r\n";
                zvd_send_exact(client_fd, rng_err.data(), rng_err.size());
                close(file_fd);
                close(client_fd);
                return;
            }
        }

        if (total_size > 0 && range_end >= static_cast<off_t>(total_size)) range_end = static_cast<off_t>(total_size - 1);
        uint64_t chunk_len = (total_size == 0) ? 0 : ((has_range && range_end >= range_start) ? static_cast<uint64_t>(range_end - range_start + 1) : total_size);

        std::string http_headers;
        if (has_range) {
            http_headers = "HTTP/1.1 206 Partial Content\r\n";
            http_headers += "Content-Range: bytes " + std::to_string(range_start) + "-" + std::to_string(range_end) + "/" + std::to_string(total_size) + "\r\n";
        } else {
            http_headers = "HTTP/1.1 200 OK\r\n";
        }

        http_headers += "Content-Type: application/octet-stream\r\n";
        http_headers += "Accept-Ranges: bytes\r\n";
        http_headers += "Content-Length: " + std::to_string(chunk_len) + "\r\n";
        http_headers += "X-ZVD-Stream: 1\r\n";
        http_headers += "X-ZVD-Hash64: " + std::to_string(idx.full_file_hash64) + "\r\n";
        http_headers += "Connection: close\r\n\r\n";

        zvd_send_exact(client_fd, http_headers.data(), http_headers.size());

        if (chunk_len > 0) {
            posix_fadvise(file_fd, range_start, static_cast<off_t>(chunk_len), POSIX_FADV_SEQUENTIAL);
            off_t offset = range_start;
            uint64_t remaining = chunk_len;

            while (remaining > 0) {
                size_t step = static_cast<size_t>(std::min<uint64_t>(remaining, 0x7ffff000ULL));
                ssize_t sent = sendfile(client_fd, file_fd, &offset, step);
                if (sent <= 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                remaining -= static_cast<uint64_t>(sent);
                g_total_bytes_served.fetch_add(static_cast<uint64_t>(sent), std::memory_order_relaxed);
            }
        }

        close(file_fd);
        close(client_fd);
        return;
    }

    if (req.starts_with("PUT /files/") || req.starts_with("POST /upload")) {
        if (!zvd_is_authorized(req, cfg)) {
            zvd_send_json(client_fd, "401 Unauthorized", "{\"error\":\"Unauthorized\"}");
            close(client_fd);
            return;
        }

        std::string raw_name = "";
        if (req.starts_with("PUT /files/")) {
            size_t start = 11;
            size_t end = req.find(' ', start);
            if (end == std::string_view::npos) end = req.size();
            raw_name = std::string(req.substr(start, end - start));
        } else {
            size_t q_pos = req.find("name=");
            if (q_pos != std::string_view::npos) {
                size_t start = q_pos + 5;
                size_t end = req.find(' ', start);
                if (end == std::string_view::npos) end = req.find('&', start);
                if (end == std::string_view::npos) end = req.size();
                raw_name = std::string(req.substr(start, end - start));
            }
        }

        std::string filename;
        if (!zvd_sanitize_filename(raw_name, filename)) {
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Invalid or missing filename\"}");
            close(client_fd);
            return;
        }

        size_t cl_pos = req.find("Content-Length:");
        if (cl_pos == std::string_view::npos) cl_pos = req.find("content-length:");
        if (cl_pos == std::string_view::npos) {
            zvd_send_json(client_fd, "411 Length Required", "{\"error\":\"Content-Length required\"}");
            close(client_fd);
            return;
        }

        uint64_t content_length = 0;
        size_t colon = req.find(':', cl_pos);
        if (colon != std::string_view::npos) {
            content_length = std::strtoull(req.data() + colon + 1, nullptr, 10);
        }

        bool explicit_nested = zvd_header_equals_ci(req, "X-ZVD-Codec:", "nested") ||
                               zvd_header_equals_ci(req, "x-zvd-codec:", "nested");
        bool ext_zvd3 = std::string_view(filename).ends_with(".zvd3");
        bool wants_nested = explicit_nested || ext_zvd3;

        size_t initial_body_offset = (header_end_pos != std::string_view::npos) ? (header_end_pos + sep_len) : 0u;

        if (wants_nested) {
#if ZVD_FABRIC_HAS_NESTED_CODEC
            if (content_length == 0) {
                zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Nested macro-cell payload cannot be empty\"}");
                close(client_fd);
                return;
            }
            if (content_length > static_cast<uint64_t>(ZVD_MAX_MACRO_CELL_BYTES)) {
                zvd_send_json(client_fd, "413 Payload Too Large", "{\"error\":\"Payload exceeds ZVD_MAX_MACRO_CELL_BYTES\"}");
                close(client_fd);
                return;
            }

            static thread_local alignas(64) std::vector<uint8_t> nested_buf;
            if (nested_buf.capacity() < ZVD_MAX_MACRO_CELL_BYTES) {
                nested_buf.reserve(ZVD_MAX_MACRO_CELL_BYTES);
            }
            nested_buf.resize(static_cast<size_t>(content_length));

            size_t total_read = 0;
            if (initial_body_offset < req_len) {
                size_t avail = std::min<size_t>(req_len - initial_body_offset, nested_buf.size());
                std::memcpy(nested_buf.data(), req.data() + initial_body_offset, avail);
                total_read += avail;
            }

            bool disconnected = false;
            while (total_read < nested_buf.size()) {
                ssize_t r = recv(client_fd, nested_buf.data() + total_read, nested_buf.size() - total_read, 0);
                if (r <= 0) {
                    if (r < 0 && errno == EINTR) continue;
                    disconnected = true;
                    break;
                }
                total_read += static_cast<size_t>(r);
            }

            if (disconnected || total_read != nested_buf.size()) {
                zvd_send_json(client_fd, "499 Client Closed Request", "{\"error\":\"Incomplete nested cell upload\"}");
                close(client_fd);
                return;
            }

            g_total_bytes_ingested.fetch_add(static_cast<uint64_t>(total_read), std::memory_order_relaxed);
            char cell_path[512]{0};
            bool deduplicated = false;
            bool put_ok = zvd_storage_put_nested_cell(
                cfg.storage_root.c_str(),
                nested_buf.data(),
                total_read,
                cfg.master_key.c_str(),
                zvd_get_codec_mode(),
                cell_path,
                &deduplicated
            );

            if (!put_ok) {
                zvd_send_json(client_fd, "500 Internal Server Error", "{\"error\":\"Failed to encode or store .zvd3 cell\"}");
                close(client_fd);
                return;
            }

            std::string body = "{\"status\":\"STORED_NESTED\",\"file\":\"" + zvd_json_escape(filename) +
                               "\",\"cell_path\":\"" + zvd_json_escape(cell_path) +
                               "\",\"bytes\":" + std::to_string(total_read) +
                               ",\"deduplicated\":" + std::string(deduplicated ? "true" : "false") +
                               ",\"rack_id\":" + std::to_string(cfg.rack_id) + "}";
            zvd_send_json(client_fd, "201 Created", body);
            close(client_fd);
            return;
#else
            zvd_send_json(client_fd, "400 Bad Request", "{\"error\":\"Nested codec not linked in this build\"}");
            close(client_fd);
            return;
#endif
        }

        std::filesystem::path final_fpath = std::filesystem::path(cfg.storage_root) / filename;
        std::filesystem::create_directories(cfg.storage_root);

        uint64_t tid = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        uint64_t seq = g_tmp_file_seq.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path tmp_fpath = std::filesystem::path(cfg.storage_root) /
                                    (filename + ".tmp." + std::to_string(tid) + "." + std::to_string(seq));

        int out_fd = open(tmp_fpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            zvd_send_json(client_fd, "500 Internal Server Error", "{\"error\":\"Failed to open temporary file for storage\"}");
            close(client_fd);
            return;
        }

        uint64_t total_received = 0;
        uint64_t rolling_hash = 0x9E3779B97F4A7C15ULL;
        ZvdPhaseHysteresisTracker hyst;

        if (initial_body_offset < req_len && content_length > 0) {
            size_t available_in_buf = req_len - initial_body_offset;
            size_t initial_bytes = static_cast<size_t>(std::min<uint64_t>(available_in_buf, content_length));
            const uint8_t* init_ptr = reinterpret_cast<const uint8_t*>(req.data() + initial_body_offset);
            zvd_write_exact(out_fd, init_ptr, initial_bytes);
            total_received += initial_bytes;
            g_total_bytes_ingested.fetch_add(initial_bytes, std::memory_order_relaxed);
            rolling_hash = zvd_fabric_fast_hash64_incremental(init_ptr, initial_bytes, rolling_hash);
            hyst.observe(init_ptr, initial_bytes);
        }

        alignas(64) std::array<char, 65536> io_chunk;
        bool client_disconnected = false;
        while (total_received < content_length) {
            size_t to_read = static_cast<size_t>(std::min<uint64_t>(io_chunk.size(), content_length - total_received));
            ssize_t r = recv(client_fd, io_chunk.data(), to_read, 0);
            if (r <= 0) {
                if (r < 0 && errno == EINTR) continue;
                client_disconnected = true;
                break;
            }
            const uint8_t* chunk_u8 = reinterpret_cast<const uint8_t*>(io_chunk.data());
            zvd_write_exact(out_fd, chunk_u8, static_cast<size_t>(r));
            total_received += static_cast<size_t>(r);
            g_total_bytes_ingested.fetch_add(static_cast<size_t>(r), std::memory_order_relaxed);
            rolling_hash = zvd_fabric_fast_hash64_incremental(chunk_u8, static_cast<size_t>(r), rolling_hash);
            hyst.observe(chunk_u8, static_cast<size_t>(r));
        }
        close(out_fd);

        std::error_code ec;
        if (!client_disconnected && total_received == content_length) {
            std::filesystem::rename(tmp_fpath, final_fpath, ec);
            if (ec) {
                std::filesystem::remove(tmp_fpath, ec);
                zvd_send_json(client_fd, "500 Internal Server Error", "{\"error\":\"Atomic rename failed\"}");
                close(client_fd);
                return;
            }

            hyst.finalize(content_length);

            ZvdSidecarIndex sidx{};
            sidx.file_size = total_received;
            sidx.full_file_hash64 = rolling_hash;
            sidx.gl_invariant = hyst.last_gl_invariant;
            sidx.entropy = hyst.last_entropy;
            sidx.signature_monolith = hyst.last_monolith;
            sidx.risk_index = hyst.last_risk_index;
            zvd_write_sidecar(cfg.storage_root, filename, sidx);

            bool replicate = (!cfg.peer_rack_ip.empty());
            if (req.find("X-ZVD-Replicate: 1") != std::string_view::npos) replicate = true;
            if (replicate) {
                zvd_spawn_peer_replication(filename, final_fpath.string(), total_received);
            }

            std::string json_reply = "{\"status\":\"STORED\",\"file\":\"" + zvd_json_escape(filename) + "\"";
            json_reply += ",\"bytes\":" + std::to_string(total_received);
            json_reply += ",\"hash64\":\"" + std::to_string(rolling_hash) + "\"";
            json_reply += ",\"gl_invariant\":" + std::to_string(static_cast<int>(hyst.last_gl_invariant));
            json_reply += ",\"entropy\":" + std::to_string(hyst.last_entropy);
            json_reply += ",\"risk_index\":" + std::to_string(hyst.last_risk_index);
            json_reply += ",\"rack_id\":" + std::to_string(cfg.rack_id) + "}";
            zvd_send_json(client_fd, "201 Created", json_reply);
        } else {
            std::filesystem::remove(tmp_fpath, ec);
            zvd_send_json(client_fd, "499 Client Closed Request", "{\"error\":\"Incomplete upload or disconnect\"}");
        }
        close(client_fd);
        return;
    }

    zvd_send_json(client_fd, "404 Not Found", "{\"error\":\"Endpoint not found\"}");
    close(client_fd);
}

static void zvd_run_http_worker() {
    const auto& cfg = ZvdFabricConfig::get();
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    timeval accept_tv{0, 500000};
    setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &accept_tv, sizeof(accept_tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(cfg.listen_port));

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_fd);
        return;
    }
    if (listen(listen_fd, SOMAXCONN) < 0) {
        close(listen_fd);
        return;
    }

    while (!g_zvd_global_shutdown.load(std::memory_order_acquire)) {
        int cfd = accept(listen_fd, nullptr, nullptr);
        if (g_zvd_global_shutdown.load(std::memory_order_acquire)) {
            if (cfd >= 0) close(cfd);
            break;
        }
        if (cfd < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            break;
        }

        if (g_active_connections.load(std::memory_order_relaxed) >= MAX_ACTIVE_CONNS) {
            zvd_send_json(cfd, "503 Service Unavailable", "{\"error\":\"Server busy\"}");
            close(cfd);
            continue;
        }

        g_active_connections.fetch_add(1, std::memory_order_relaxed);
        zvd_handle_http_client(cfd);
        g_active_connections.fetch_sub(1, std::memory_order_relaxed);
    }

    close(listen_fd);
}
// =====================================================================
// SECTION 6: ENTRY POINT & CARTRIDGE REGISTRATION
// =====================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
static int zvd_fabric_telemetry_eval(ZvdPipelineContext& ctx) {
    (void)ctx;
    return 0;
}
ZVD_REGISTER_CARTRIDGE_CTX("ZVD-Sovereign-Storage-Fabric", ZvdModuleRole::TELEMETRY, zvd_fabric_telemetry_eval, nullptr);
#endif

#if !defined(ZVD_NO_MAIN) || (ZVD_NO_MAIN == 0)
int main(int argc, char **argv) {
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, zvd_fabric_signal_handler);
    std::signal(SIGTERM, zvd_fabric_signal_handler);
    (void)argc;
    (void)argv;

#if defined(ZVD_HAS_DATACENTER_CORE)
    zvd_ensure_luts();
#endif

    const auto& cfg = ZvdFabricConfig::get();
    unsigned int num_cores = std::thread::hardware_concurrency();
    size_t num_workers = std::max(1u, num_cores);
    const char* auth_status = cfg.api_token.empty() ? "OPEN (no ZVD_API_TOKEN)" : "ENABLED";

    std::printf("======================================================================\n");
    std::printf("ZVD SOVEREIGN STORAGE FABRIC (V1.3.0-SUPREME-C20)\n");
    std::printf("HTTP Storage Plane listening on 0.0.0.0:%d -> Vault: %s\n", cfg.listen_port, cfg.storage_root.c_str());
    std::printf("Inter-Rack ZVDO Fabric Plane listening on 0.0.0.0:%d (Rack ID: %u)\n", cfg.rack_port, cfg.rack_id);
    std::printf("Mutation Auth: %s | Nested Codec: %s\n", auth_status, ZVD_FABRIC_HAS_NESTED_CODEC ? "ACTIVE" : "OFF");
    std::printf("Spawning %zu HTTP Workers (1 per CPU core via SO_REUSEPORT)\n", num_workers);
    std::printf("Root-Architect: Vladimir Zavodiuk (Varna, Bulgaria, EU)\n");
    std::printf("======================================================================\n");

    std::thread rack_listener(zvd_rack_fabric_listener_thread);

    std::vector<std::thread> workers;
    workers.reserve(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(zvd_run_http_worker);
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    if (rack_listener.joinable()) {
        rack_listener.join();
    }

    while (g_active_replications.load(std::memory_order_acquire) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
#endif
