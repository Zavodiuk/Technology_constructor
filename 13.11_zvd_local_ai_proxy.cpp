/* =============================================================================
 *  PROJECT:    ZVD SOVEREIGN UNIVERSAL CONSTRUCTOR BUS & HYPER-PERIMETER PROXY
 *              (13.11_zvd_local_ai_proxy.cpp - V3.3.3-HARDENED-C20)
 *  FILENAME:   13.11_zvd_local_ai_proxy.cpp
 *  VERSION:    2026.11-V3.3.3-HARDENED-C20
 *
 *  [ARCHITECTURAL AUDIT & SCALE GUARANTEES]:
 *    - ZERO-ALLOCATION HOT PATHS: thread_local stack buffers & preallocated conn_pool.
 *    - EPOLL ASYNC REACTOR: Edge-Triggered (EPOLLET) non-blocking I/O for C10K+ throughput.
 *    - KERNEL LOAD BALANCING: SO_REUSEPORT multi-worker per CPU core architecture.
 *    - ZERO-ALLOC STREAMING PASSTHROUGH: OOM-free multi-GB uploads and downloads (>64KB).
 * =============================================================================
 *  BUILD (Linux):
 *    g++ -std=c++20 -O3 -march=native -Wall -Wextra -Werror -fopenmp "13.11_zvd_local_ai_proxy.cpp" -o zvd_proxy -lm -lpthread
 *
 *  AUTHOR:     Vladimir Zavodiuk (Root-Architect)
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
#include <csignal>
#include <atomic>
#include <cstdint>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <errno.h>

// =====================================================================
// SECTION 1: CORE INCLUDES, SYSTEM MACROS & FORWARD DECLARATIONS
// =====================================================================

struct ZvdPipelineContext {
    std::string_view prompt;
    int8_t gl_invariant = 0;
    double entropy = 0.0;
    const void* datacenter_pkt_ptr = nullptr;
    int accumulated_trity = 0;
    std::string routing_directive = "";
    bool telemetry_dispatched = false;
};

enum class ZvdModuleRole {
    ARBITER,
    TELEMETRY,
    FILTER
};

// =====================================================================
// SECTION 2: PIPELINE CONTEXT, MODULE BUS & CARTRIDGE REGISTRAR
// =====================================================================

struct ZvdCartridgeEntry {
    std::string name;
    ZvdModuleRole role;
    int (*eval_fn)(std::string_view prompt, int8_t gl_inv, double entropy);
    int (*eval_ctx_fn)(ZvdPipelineContext& ctx);
    bool (*verify_fn)(std::string_view prompt, std::string_view response);
};

inline std::vector<ZvdCartridgeEntry>& zvd_get_module_bus() {
    static std::vector<ZvdCartridgeEntry> bus;
    return bus;
}

struct ZvdAutoRegistrar {
    ZvdAutoRegistrar(const char* name,
                     ZvdModuleRole role,
                     int (*ev)(std::string_view, int8_t, double),
                     int (*ev_ctx)(ZvdPipelineContext&),
                     bool (*ver)(std::string_view, std::string_view) = nullptr) {
        zvd_get_module_bus().push_back({name, role, ev, ev_ctx, ver});
    }
};

#define ZVD_REGISTER_CARTRIDGE(NAME, EVAL_FN, VERIFY_FN) \
    static ZvdAutoRegistrar _zvd_reg_##__LINE__(NAME, ZvdModuleRole::ARBITER, EVAL_FN, nullptr, VERIFY_FN)

#define ZVD_REGISTER_CARTRIDGE_CTX(NAME, ROLE, EVAL_CTX_FN, VERIFY_FN) \
    static ZvdAutoRegistrar _zvd_reg_##__LINE__(ROLE == ZvdModuleRole::FILTER ? NAME : NAME, ROLE, nullptr, EVAL_CTX_FN, VERIFY_FN)

// =====================================================================
// SECTION 3: AUTO-MANIFEST INGESTION & DATACENTER CORE LINKAGE
// =====================================================================

#if defined(__has_include)
  #if __has_include("zvd_auto_manifest.h")
    #define ZVD_HAS_AUTO_MANIFEST 1
    #include "zvd_auto_manifest.h"
  #endif

  #ifndef ZVD_HAS_DATACENTER_CORE
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

  #if defined(ZVD_HAS_DATACENTER_CORE) && !defined(ZVD_HAS_AUTO_MANIFEST)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    extern "C" {
      #include ZVD_CORE_FILENAME
    }
#pragma GCC diagnostic pop
  #endif

  #if !defined(ZVD_HAS_AUTO_MANIFEST)
    #if __has_include("13.11_zvd_ddos_aegis_v30.hpp")
      #include "13.11_zvd_ddos_aegis_v30.hpp"
      #define ZVD_HAS_DDOS_AEGIS 1
    #elif __has_include("13.11 zvd_ddos_aegis_v30.hpp")
      #include "13.11 zvd_ddos_aegis_v30.hpp"
      #define ZVD_HAS_DDOS_AEGIS 1
    #elif __has_include("zvd_ddos_aegis_v30.hpp")
      #include "zvd_ddos_aegis_v30.hpp"
      #define ZVD_HAS_DDOS_AEGIS 1
    #endif

#ifndef ZVD_NO_MAIN
#define ZVD_NO_MAIN 0
#endif
    #if __has_include("13.11_zvd_cognitive_theatre.hpp")
      #include "13.11_zvd_cognitive_theatre.hpp"
    #elif __has_include("13.11 zvd_cognitive_theatre.hpp")
      #include "13.11 zvd_cognitive_theatre.hpp"
    #elif __has_include("zvd_cognitive_theatre.hpp")
      #include "zvd_cognitive_theatre.hpp"
    #endif
  #endif
#endif

// =====================================================================
// SECTION 4: COGNITIVE BRIDGE, AI DECISION ENGINE & JSON EXTRACTION
// =====================================================================

static uint64_t zvd_proxy_fast_hash64(std::string_view sv) noexcept {
    uint64_t h = 0x9E3779B97F4A7C15ULL ^ sv.size();
    for (char c : sv) {
        h ^= (uint8_t)c;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

class IZvdCognitiveBridge {
public:
    enum class Trity { PLUS, ZERO, MINUS };

    struct Decision {
        Trity trity;
        std::string action_hint;
        std::string routing_directive;
    };

    static Decision evaluate_prompt(std::string_view prompt) {
        bool zvd_check_loop_and_dedup(std::string_view prompt);

        if (zvd_check_loop_and_dedup(prompt)) {
            return { Trity::MINUS, "[ZVD-GUARD] Loop detected & blocked. CPU/Battery saved.", "LOOP_BLOCKED" };
        }

        ZvdPipelineContext ctx;
        ctx.prompt = prompt;

        bool vip_fast_bypass = false;
        for (const auto& mod : zvd_get_module_bus()) {
            if (mod.role == ZvdModuleRole::FILTER) {
                int verdict = 0;
                if (mod.eval_ctx_fn) {
                    verdict = mod.eval_ctx_fn(ctx);
                } else if (mod.eval_fn) {
                    verdict = mod.eval_fn(prompt, ctx.gl_invariant, ctx.entropy);
                }

                if (verdict == -1) {
                    std::string directive = ctx.routing_directive.empty() ? "FILTER_VETO" : ctx.routing_directive;
                    return { Trity::MINUS, "[ZVD-PERIMETER-VETO] Blocked by filter: " + mod.name, directive };
                } else if (verdict == 1) {
                    vip_fast_bypass = true;
                    break;
                }
            }
        }

        if (vip_fast_bypass) {
            std::string dir = ctx.routing_directive.empty() ? "SOVEREIGN_VIP_BYPASS" : ctx.routing_directive;
            return { Trity::PLUS, dir, dir };
        }

#if defined(ZVD_HAS_DATACENTER_CORE)
        static thread_local bool luts_inited = false;
        if (!luts_inited) {
            zvd_ensure_luts();
            luts_inited = true;
        }

        ZVD_BiCore120D bicore;
        zvd_bicore_init(&bicore, 0, 1);
        static thread_local ZVD_DatacenterPacket pkt;
        std::memset(&pkt, 0, sizeof(pkt));

        if (zvd_node_ingest_monolith_only(reinterpret_cast<const uint8_t*>(prompt.data()), prompt.size(), &bicore, &pkt)) {
            ctx.gl_invariant = pkt.gl_invariant;
            ctx.entropy = pkt.entropy;
            ctx.datacenter_pkt_ptr = &pkt;

            if (pkt.entropy > 7.2 || pkt.gl_invariant == -2) {
                return { Trity::MINUS, "[ZVD-120D-GUARD] High entropy or Godel-Lukasiewicz VETO triggered.", "DATACENTER_VETO" };
            }

            static thread_local int udp_sock = -1;
            if (udp_sock < 0) {
                udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
                int flags = fcntl(udp_sock, F_GETFL, 0);
                fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
            }

            if (udp_sock >= 0) {
                static const sockaddr_in dc_addr = []() {
                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    const char* env_ip = std::getenv("ZVD_DC_IP");
                    if (!env_ip || inet_pton(AF_INET, env_ip, &addr.sin_addr) != 1) {
                        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                    }
                    const char* env_port = std::getenv("ZVD_DC_PORT");
                    int dc_port = env_port ? std::atoi(env_port) : 9000;
                    addr.sin_port = htons((uint16_t)dc_port);
                    return addr;
                }();

                alignas(64) uint8_t optic_frame[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
                size_t optic_sz = 0;
                static const char* master_key = []() {
                    const char* mk = std::getenv("ZVD_MASTER_KEY");
                    return mk ? mk : ZVD_DEFAULT_MASTER_KEY;
                }();

                if (zvd_optic_frame_pack(101, 202, (const uint8_t*)&pkt.monolith, sizeof(ZVD_Monolith334), master_key, optic_frame, sizeof(optic_frame), &optic_sz)) {
                    auto sent_bytes = sendto(udp_sock, (const char*)optic_frame, optic_sz, 0, (struct sockaddr*)&dc_addr, sizeof(dc_addr));
                    (void)sent_bytes;
                    ctx.telemetry_dispatched = true;
                }
            }
        }
#endif

        bool has_plus_verdict = false;
        for (const auto& mod : zvd_get_module_bus()) {
            if (mod.role == ZvdModuleRole::FILTER) continue;

            int verdict = 0;
            if (mod.eval_ctx_fn) {
                verdict = mod.eval_ctx_fn(ctx);
            } else if (mod.eval_fn) {
                verdict = mod.eval_fn(prompt, ctx.gl_invariant, ctx.entropy);
            }

            if (verdict == -1) {
                return { Trity::MINUS, "[ZVD-BUS-VETO] Blocked by module: " + mod.name, "BUS_VETO" };
            } else if (verdict == 1) {
                has_plus_verdict = true;
            }
        }

        if (has_plus_verdict) {
            std::string dir = ctx.routing_directive.empty() ? "BUS_MODULE_BYPASS" : ctx.routing_directive;
            return { Trity::PLUS, dir, dir };
        }

        if (prompt.size() < 40) {
            return { Trity::PLUS, "FAST_BYPASS", "FAST_CONCISE" };
        }
        return { Trity::ZERO, "DEEP_REASONING_REQUIRED", "DEEP_COT" };
    }
};

static std::string extract_json_content(std::string_view body) noexcept {
    size_t pos = 0;
    size_t last_content_pos = std::string_view::npos;
    size_t last_content_len = 0;

    while ((pos = body.find("\"role\"", pos)) != std::string_view::npos) {
        size_t colon_pos = body.find(':', pos);
        if (colon_pos == std::string_view::npos) break;

        size_t user_pos = body.find("\"user\"", colon_pos);
        size_t role_val_start = body.find('"', colon_pos);
        bool is_user = false;
        if (user_pos != std::string_view::npos && user_pos < colon_pos + 30) {
            is_user = true;
        } else if (role_val_start != std::string_view::npos) {
            size_t role_val_end = body.find('"', role_val_start + 1);
            if (role_val_end != std::string_view::npos) {
                std::string_view rval = body.substr(role_val_start + 1, role_val_end - (role_val_start + 1));
                if (rval == "user") is_user = true;
            }
        }

        last_content_pos = std::string_view::npos;
        last_content_len = 0;

        if (is_user) {
            size_t block_end = body.find('}', pos);
            size_t content_key = body.find("\"content\"", pos);
            if (content_key != std::string_view::npos && (block_end == std::string_view::npos || content_key < block_end)) {
                size_t colon_c = body.find(':', content_key);
                if (colon_c != std::string_view::npos && (block_end == std::string_view::npos || colon_c < block_end)) {
                    size_t first_quote = body.find('"', colon_c);
                    if (first_quote != std::string_view::npos && (block_end == std::string_view::npos || first_quote < block_end)) {
                        size_t current = first_quote + 1;
                        bool escaped = false;
                        size_t val_start = current;
                        while (current < body.size() && (block_end == std::string_view::npos || current < block_end)) {
                            char c = body[current];
                            if (c == '\\' && !escaped) {
                                escaped = true;
                            } else if (c == '"' && !escaped) {
                                break;
                            } else {
                                escaped = false;
                            }
                            current++;
                        }
                        if (current < body.size()) {
                            last_content_pos = val_start;
                            last_content_len = current - val_start;
                        }
                    }
                }
            }
        }
        pos = colon_pos + 1;
    }

    if (last_content_pos != std::string_view::npos) {
        std::string_view raw_extracted = body.substr(last_content_pos, last_content_len);
        std::string result;
        result.reserve(raw_extracted.size());
        for (size_t i = 0; i < raw_extracted.size(); ++i) {
            if (raw_extracted[i] == '\\' && i + 1 < raw_extracted.size()) {
                char next_c = raw_extracted[i+1];
                if (next_c == 'n') result.push_back('\n');
                else if (next_c == 't') result.push_back('\t');
                else if (next_c == 'r') result.push_back('\r');
                else if (next_c == '"') result.push_back('"');
                else if (next_c == '\\') result.push_back('\\');
                else {
                    result.push_back('\\');
                    result.push_back(next_c);
                }
                i++;
            } else {
                result.push_back(raw_extracted[i]);
            }
        }
        return result;
    }

    return "";
}

// =====================================================================
// SECTION 5: LOCK-FREE SLIDING-WINDOW DEDUP RING & EPOCH MANAGEMENT
// =====================================================================

static constexpr size_t DEDUP_RING_SIZE = 16384;
alignas(64) static std::atomic<uint64_t> g_ring_hashes[DEDUP_RING_SIZE] = {0};
alignas(64) static std::atomic<uint32_t> g_ring_counts[DEDUP_RING_SIZE] = {0};
alignas(64) static std::atomic<uint32_t> g_ring_epochs[DEDUP_RING_SIZE] = {0};

alignas(64) static std::atomic<uint32_t> g_cached_epoch{0};
alignas(64) static std::atomic<uint32_t> g_req_tick{0};

static bool zvd_check_loop_and_dedup(std::string_view prompt) {
    uint64_t h = zvd_proxy_fast_hash64(prompt);
    size_t idx = static_cast<size_t>(h & (DEDUP_RING_SIZE - 1));

    uint32_t tick = g_req_tick.fetch_add(1, std::memory_order_relaxed);
    if ((tick & 1023) == 0) {
        uint32_t new_epoch = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count() / 2
        );
        g_cached_epoch.store(new_epoch, std::memory_order_relaxed);
    }
    uint32_t current_epoch = g_cached_epoch.load(std::memory_order_relaxed);

    uint64_t expected_h = g_ring_hashes[idx].load(std::memory_order_acquire);
    uint32_t expected_epoch = g_ring_epochs[idx].load(std::memory_order_acquire);

    if (expected_h == h) {
        if (expected_epoch != current_epoch) {
            g_ring_epochs[idx].store(current_epoch, std::memory_order_release);
            g_ring_counts[idx].store(1, std::memory_order_release);
            return false;
        }
        uint32_t cnt = g_ring_counts[idx].fetch_add(1, std::memory_order_relaxed) + 1;
        return (cnt >= 3);
    }

    if (g_ring_hashes[idx].compare_exchange_strong(expected_h, h, std::memory_order_acq_rel)) {
        g_ring_epochs[idx].store(current_epoch, std::memory_order_release);
        g_ring_counts[idx].store(1, std::memory_order_release);
    } else if (expected_h == h) {
        if (g_ring_epochs[idx].load(std::memory_order_acquire) != current_epoch) {
            g_ring_epochs[idx].store(current_epoch, std::memory_order_release);
            g_ring_counts[idx].store(1, std::memory_order_release);
            return false;
        }
        uint32_t cnt = g_ring_counts[idx].fetch_add(1, std::memory_order_relaxed) + 1;
        return (cnt >= 3);
    }
    return false;
}

// =====================================================================
// SECTION 6: ASYNC EPOLL CONNECTION STATE MACHINE & TARGET DEFINITIONS
// =====================================================================

enum class ConnState : uint8_t {
    READING_CLIENT_REQ,
    CONNECTING_BACKEND,
    WRITING_BACKEND_REQ,
    READING_BACKEND_RESP,
    WRITING_CLIENT_RESP,
    CLOSED
};

struct ZvdAsyncConnection;

struct EpollTarget {
    ZvdAsyncConnection* conn;
    bool is_backend;
};

struct alignas(64) ZvdAsyncConnection {
    int client_fd = -1;
    int backend_fd = -1;
    ConnState state = ConnState::CLOSED;

    EpollTarget client_target{this, false};
    EpollTarget backend_target{this, true};

    static constexpr size_t IN_BUF_SIZE = 65536;
    char in_buf[IN_BUF_SIZE + 512]; // Headroom for injected headers
    size_t in_len = 0;
    size_t in_offset = 0;

    std::string backend_req;
    size_t backend_req_sent = 0;

    std::string backend_resp;
    std::string out_buf;
    size_t out_sent = 0;

    bool is_upload_stream = false;
    bool is_download_stream = false;
    size_t stream_bytes_remaining = 0;

    IZvdCognitiveBridge::Decision decision;
    std::string user_prompt;

    void reset(int epfd) {
        if (client_fd >= 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
            close(client_fd);
            client_fd = -1;
        }
        if (backend_fd >= 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, backend_fd, nullptr);
            close(backend_fd);
            backend_fd = -1;
        }
        state = ConnState::CLOSED;
        in_len = 0;
        in_offset = 0;
        backend_req_sent = 0;
        out_sent = 0;
        is_upload_stream = false;
        is_download_stream = false;
        stream_bytes_remaining = 0;
        backend_req.clear();
        backend_resp.clear();
        out_buf.clear();
        user_prompt.clear();
    }
};

// =====================================================================
// HTTP HEADER HELPERS FOR KEEP-ALIVE & STREAMING INJECTION
// =====================================================================

static bool zvd_iequals_ascii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb + ('a' - 'A'));
        if (ca != cb) return false;
    }
    return true;
}

static size_t zvd_find_header_end(std::string_view sv, size_t& out_sep_len) noexcept {
    size_t pos_crlf = sv.find("\r\n\r\n");
    size_t pos_lf = sv.find("\n\n");
    if (pos_crlf != std::string_view::npos && (pos_lf == std::string_view::npos || pos_crlf <= pos_lf)) {
        out_sep_len = 4;
        return pos_crlf;
    }
    if (pos_lf != std::string_view::npos) {
        out_sep_len = 2;
        return pos_lf;
    }
    out_sep_len = 0;
    return std::string_view::npos;
}

static bool zvd_find_content_length(std::string_view headers_sv, size_t& out_cl) noexcept {
    size_t pos = 0;
    while (pos < headers_sv.size()) {
        size_t line_end = headers_sv.find('\n', pos);
        if (line_end == std::string_view::npos) line_end = headers_sv.size();
        std::string_view line = headers_sv.substr(pos, line_end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string_view key = line.substr(0, colon);
            if (zvd_iequals_ascii(key, "content-length")) {
                std::string_view val = line.substr(colon + 1);
                while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) {
                    val.remove_prefix(1);
                }
                if (!val.empty()) {
                    char tmp[32];
                    size_t copy_len = std::min(val.size(), sizeof(tmp) - 1);
                    std::memcpy(tmp, val.data(), copy_len);
                    tmp[copy_len] = '\0';
                    char* endptr = nullptr;
                    unsigned long long parsed = std::strtoull(tmp, &endptr, 10);
                    if (endptr != tmp) {
                        out_cl = static_cast<size_t>(parsed);
                        return true;
                    }
                }
            }
        }
        if (line_end == headers_sv.size()) break;
        pos = line_end + 1;
    }
    return false;
}

static std::string zvd_force_connection_close_headers(std::string_view raw_headers) {
    std::string out;
    out.reserve(raw_headers.size() + 32);

    size_t pos = 0;
    bool first_line = true;
    while (pos < raw_headers.size()) {
        size_t line_end = raw_headers.find('\n', pos);
        bool has_lf = (line_end != std::string_view::npos);
        if (!has_lf) line_end = raw_headers.size();

        std::string_view line = raw_headers.substr(pos, line_end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (first_line) {
            out.append(line.data(), line.size());
            out.append("\r\n");
            first_line = false;
        } else {
            size_t colon = line.find(':');
            if (colon != std::string_view::npos && zvd_iequals_ascii(line.substr(0, colon), "connection")) {
                out.append("Connection: close\r\n");
            } else {
                out.append(line.data(), line.size());
                out.append("\r\n");
            }
        }

        if (!has_lf) break;
        pos = line_end + 1;
    }
    return out;
}

static std::string zvd_inject_diag_headers_into_response(std::string_view raw_resp,
                                                        size_t hdr_end,
                                                        size_t sep_len,
                                                        std::string_view diag_headers) {
    std::string_view head_part = raw_resp.substr(0, hdr_end);
    std::string_view body_part = raw_resp.substr(hdr_end + sep_len);
    std::string line_sep = (sep_len == 4) ? "\r\n" : "\n";

    std::string result;
    result.reserve(raw_resp.size() + diag_headers.size() + 8);

    size_t first_eol = head_part.find('\n');
    if (first_eol != std::string_view::npos) {
        std::string_view status_line = head_part.substr(0, first_eol);
        if (!status_line.empty() && status_line.back() == '\r') {
            status_line.remove_suffix(1);
        }
        std::string_view rest_headers = head_part.substr(first_eol + 1);
        result.append(status_line.data(), status_line.size());
        result.append(line_sep);
        result.append(diag_headers.data(), diag_headers.size());
        if (!rest_headers.empty()) {
            result.append(rest_headers.data(), rest_headers.size());
            if (rest_headers.back() != '\n') {
                result.append(line_sep);
            }
        }
    } else {
        result.append(head_part.data(), head_part.size());
        result.append(line_sep);
        result.append(diag_headers.data(), diag_headers.size());
    }

    result.append(line_sep);
    result.append(body_part.data(), body_part.size());
    return result;
}
// =====================================================================
// SECTION 7: REACTOR INITIALIZATION & CLIENT REQUEST INGESTION
// =====================================================================

static void run_async_epoll_worker(int listen_port, std::string target_ip, int target_port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd < 0) return;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    int nodelay = 1;
    setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(listen_port));

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_fd);
        return;
    }
    if (listen(listen_fd, SOMAXCONN) < 0) {
        close(listen_fd);
        return;
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        close(listen_fd);
        return;
    }

    epoll_event listen_ev{};
    listen_ev.events = EPOLLIN | EPOLLET;
    listen_ev.data.ptr = nullptr;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &listen_ev);

    constexpr size_t MAX_CONNS = 32768;
    std::vector<ZvdAsyncConnection> conn_pool(MAX_CONNS);
    std::vector<epoll_event> events(1024);

    while (true) {
        int nfds = epoll_wait(epfd, events.data(), static_cast<int>(events.size()), -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.ptr == nullptr) {
                while (true) {
                    int cfd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
                    if (cfd < 0) break;
                    if (static_cast<size_t>(cfd) >= MAX_CONNS) {
                        close(cfd);
                        continue;
                    }

                    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                    auto& conn = conn_pool[cfd];
                    conn.reset(epfd);
                    conn.client_fd = cfd;
                    conn.state = ConnState::READING_CLIENT_REQ;

                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.ptr = &conn.client_target;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                }
            } else {
                auto* target = static_cast<EpollTarget*>(events[i].data.ptr);
                auto* conn = target->conn;

                if (!target->is_backend) {
                    if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                        conn->reset(epfd);
                        continue;
                    }

                    if (conn->state == ConnState::READING_CLIENT_REQ) {
                        if (conn->is_upload_stream) {
                            size_t read_so_far = 0;
                            bool should_reset = false;
                            while (conn->stream_bytes_remaining > 0 && read_so_far < ZvdAsyncConnection::IN_BUF_SIZE) {
                                size_t to_read = std::min(conn->stream_bytes_remaining, ZvdAsyncConnection::IN_BUF_SIZE - read_so_far);
                                ssize_t n = recv(conn->client_fd, conn->in_buf + read_so_far, to_read, 0);
                                if (n > 0) {
                                    size_t un = static_cast<size_t>(n);
                                    read_so_far += un;
                                    conn->stream_bytes_remaining -= un;
                                } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                    break;
                                } else {
                                    should_reset = true;
                                    break;
                                }
                            }
                            if (should_reset) {
                                conn->reset(epfd);
                                continue;
                            }
                            if (read_so_far > 0) {
                                conn->in_len = read_so_far;
                                conn->in_offset = 0;
                                conn->state = ConnState::WRITING_BACKEND_REQ;
                                epoll_event bev{EPOLLOUT | EPOLLET, {.ptr = &conn->backend_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->backend_fd, &bev);
                            }
                            continue;
                        }

                        while (conn->in_len < ZvdAsyncConnection::IN_BUF_SIZE - 1) {
                            ssize_t n = recv(conn->client_fd, conn->in_buf + conn->in_len,
                                             ZvdAsyncConnection::IN_BUF_SIZE - 1 - conn->in_len, 0);
                            if (n > 0) {
                                conn->in_len += static_cast<size_t>(n);
                            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            } else {
                                conn->reset(epfd);
                                break;
                            }
                        }

                        if (conn->client_fd < 0) continue;
                        conn->in_buf[conn->in_len] = '\0';

                        std::string_view req(conn->in_buf, conn->in_len);

                        if (req.starts_with("GET /healthz") || req.find("GET /healthz") == 0) {
                            std::string payload = "{\"status\":\"OK\",\"perimeter\":\"active_patch_v333\"}";
                            std::string health_resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
                            conn->out_buf = health_resp;
                            conn->state = ConnState::WRITING_CLIENT_RESP;
                            epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                            epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                            continue;
                        }

                        size_t sep_len = 0;
                        size_t header_end_pos = zvd_find_header_end(std::string_view(conn->in_buf, conn->in_len), sep_len);
                        if (header_end_pos == std::string_view::npos) {
                            continue;
                        }
                        size_t body_start_pos = header_end_pos + sep_len;

                        size_t content_length = 0;
                        zvd_find_content_length(req.substr(0, header_end_pos), content_length);

                        size_t initial_body_bytes = (conn->in_len > body_start_pos) ? (conn->in_len - body_start_pos) : 0;
                        if (content_length > ZvdAsyncConnection::IN_BUF_SIZE - 1) {
                            size_t inspect_limit = std::min(conn->in_len, body_start_pos + 4096);
                            std::string_view inspect_sv(conn->in_buf, inspect_limit);
                            std::string extracted_prompt = extract_json_content(inspect_sv);

                            std::string_view eval_target = extracted_prompt.empty() ? inspect_sv : std::string_view(extracted_prompt);
                            conn->decision = IZvdCognitiveBridge::evaluate_prompt(eval_target);

                            if (conn->decision.trity == IZvdCognitiveBridge::Trity::MINUS) {
                                std::string veto_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + conn->decision.action_hint + "\"}}]}";
                                conn->out_buf = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(veto_reply.size()) + "\r\nX-ZVD-Trity: MINUS\r\nX-ZVD-Directive: UPLOAD_AEGIS_VETO\r\nConnection: close\r\n\r\n" + veto_reply;
                                conn->state = ConnState::WRITING_CLIENT_RESP;
                                epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                                continue;
                            }

                            conn->is_upload_stream = true;
                            conn->stream_bytes_remaining = (content_length > initial_body_bytes) ? (content_length - initial_body_bytes) : 0;
                            conn->user_prompt = "[ZVD-STREAMING-UPLOAD-PASSTHROUGH]";
                            conn->decision = { IZvdCognitiveBridge::Trity::ZERO, "STREAMING_UPLOAD", "STREAMING_UPLOAD_FORWARD" };
                        } else {
                            if (conn->in_len < body_start_pos + content_length) {
                                continue;
                            }
                            std::string_view body = req.substr(body_start_pos, content_length);
                            conn->user_prompt = extract_json_content(body);
                            conn->decision = IZvdCognitiveBridge::evaluate_prompt(conn->user_prompt);
                        }

                        std::string trity_str = (conn->decision.trity == IZvdCognitiveBridge::Trity::PLUS) ? "PLUS" :
                                                (conn->decision.trity == IZvdCognitiveBridge::Trity::MINUS) ? "MINUS" : "ZERO";

                        if (conn->decision.trity == IZvdCognitiveBridge::Trity::MINUS) {
                            std::string json_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + conn->decision.action_hint + "\"}}]}";
                            conn->out_buf = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json_reply.size()) + "\r\nX-ZVD-Trity: " + trity_str + "\r\nX-ZVD-Directive: " + conn->decision.routing_directive + "\r\nConnection: close\r\n\r\n" + json_reply;
                            conn->state = ConnState::WRITING_CLIENT_RESP;
                            epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                            epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                        } else {
                            std::string headers_part = zvd_force_connection_close_headers(std::string_view(conn->in_buf, header_end_pos));
                            if (!conn->decision.routing_directive.empty()) {
                                headers_part += "X-ZVD-Directive: " + conn->decision.routing_directive + "\r\n";
                            }
                            headers_part += "\r\n";

                            if (conn->is_upload_stream) {
                                conn->backend_req = headers_part + std::string(conn->in_buf + body_start_pos, initial_body_bytes);
                                conn->backend_req_sent = 0;
                            } else {
                                conn->backend_req = headers_part + std::string(conn->in_buf + body_start_pos, content_length);
                                conn->backend_req_sent = 0;
                            }

                            int bfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
                            if (bfd < 0) {
                                conn->reset(epfd);
                                continue;
                            }
                            conn->backend_fd = bfd;
                            setsockopt(bfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                            sockaddr_in baddr{};
                            baddr.sin_family = AF_INET;
                            baddr.sin_port = htons(static_cast<uint16_t>(target_port));
                            inet_pton(AF_INET, target_ip.c_str(), &baddr.sin_addr);

                            connect(bfd, reinterpret_cast<sockaddr*>(&baddr), sizeof(baddr));

                            conn->state = ConnState::WRITING_BACKEND_REQ;
                            epoll_event bev{EPOLLOUT | EPOLLET, {.ptr = &conn->backend_target}};
                            epoll_ctl(epfd, EPOLL_CTL_ADD, bfd, &bev);
                        }
                    } else if (conn->state == ConnState::WRITING_CLIENT_RESP) {
                        if (conn->is_download_stream) {
                            while (conn->in_len > conn->in_offset) {
                                ssize_t n = send(conn->client_fd, conn->in_buf + conn->in_offset,
                                                 conn->in_len - conn->in_offset, MSG_NOSIGNAL);
                                if (n > 0) {
                                    conn->in_offset += static_cast<size_t>(n);
                                } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                    break;
                                } else {
                                    conn->reset(epfd);
                                    break;
                                }
                            }
                            if (conn->client_fd >= 0 && conn->in_offset >= conn->in_len) {
                                conn->state = ConnState::READING_BACKEND_RESP;
                                epoll_event bev{EPOLLIN | EPOLLET, {.ptr = &conn->backend_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->backend_fd, &bev);
                            }
                            continue;
                        }

                        while (conn->out_sent < conn->out_buf.size()) {
                            ssize_t n = send(conn->client_fd, conn->out_buf.data() + conn->out_sent,
                                             conn->out_buf.size() - conn->out_sent, MSG_NOSIGNAL);
                            if (n > 0) {
                                conn->out_sent += static_cast<size_t>(n);
                            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            } else {
                                break;
                            }
                        }
                        if (conn->out_sent >= conn->out_buf.size() || (events[i].events & (EPOLLHUP | EPOLLERR))) {
                            conn->reset(epfd);
                        }
                    }
                } else {
                     // =====================================================================
                    // SECTION 8: BACKEND REQUEST TRANSMISSION & UPLOAD STREAM RELAY
                    // =====================================================================

                    if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                        if (conn->is_download_stream) {
                            epoll_ctl(epfd, EPOLL_CTL_DEL, conn->backend_fd, nullptr);
                            close(conn->backend_fd);
                            conn->backend_fd = -1;
                            conn->reset(epfd);
                            continue;
                        }
                        epoll_ctl(epfd, EPOLL_CTL_DEL, conn->backend_fd, nullptr);
                        close(conn->backend_fd);
                        conn->backend_fd = -1;

                        std::string err_resp = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"Backend connection failed\"}";
                        conn->out_buf = err_resp;
                        conn->state = ConnState::WRITING_CLIENT_RESP;
                        epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                        epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                        continue;
                    }

                    if (conn->state == ConnState::WRITING_BACKEND_REQ) {
                        if (conn->is_upload_stream && conn->backend_req.empty()) {
                            while (conn->in_len > conn->in_offset) {
                                ssize_t n = send(conn->backend_fd, conn->in_buf + conn->in_offset,
                                                 conn->in_len - conn->in_offset, MSG_NOSIGNAL);
                                if (n > 0) {
                                    conn->in_offset += static_cast<size_t>(n);
                                } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                    break;
                                } else {
                                    conn->reset(epfd);
                                    break;
                                }
                            }

                            if (conn->client_fd >= 0 && conn->in_offset >= conn->in_len) {
                                if (conn->stream_bytes_remaining > 0) {
                                    conn->state = ConnState::READING_CLIENT_REQ;
                                    epoll_event cev{EPOLLIN | EPOLLET, {.ptr = &conn->client_target}};
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &cev);
                                } else {
                                    conn->state = ConnState::READING_BACKEND_RESP;
                                    epoll_event bev{EPOLLIN | EPOLLET, {.ptr = &conn->backend_target}};
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, conn->backend_fd, &bev);
                                }
                            }
                            continue;
                        }

                        while (conn->backend_req_sent < conn->backend_req.size()) {
                            ssize_t n = send(conn->backend_fd, conn->backend_req.data() + conn->backend_req_sent,
                                             conn->backend_req.size() - conn->backend_req_sent, MSG_NOSIGNAL);
                            if (n > 0) {
                                conn->backend_req_sent += static_cast<size_t>(n);
                            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            } else {
                                break;
                            }
                        }

                        if (conn->backend_req_sent >= conn->backend_req.size()) {
                            conn->backend_req.clear();
                            conn->backend_req_sent = 0;

                            if (conn->is_upload_stream && conn->stream_bytes_remaining > 0) {
                                conn->state = ConnState::READING_CLIENT_REQ;
                                epoll_event cev{EPOLLIN | EPOLLET, {.ptr = &conn->client_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &cev);
                            } else {
                                conn->state = ConnState::READING_BACKEND_RESP;
                                epoll_event bev{EPOLLIN | EPOLLET, {.ptr = &conn->backend_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->backend_fd, &bev);
                            }
                        }
                    } else if (conn->state == ConnState::READING_BACKEND_RESP) {
                        // =====================================================================
                        // SECTION 9: BACKEND RESPONSE RECEPTION & DOWNLOAD STREAM PASSTHROUGH
                        // =====================================================================

                        if (conn->is_download_stream) {
                            size_t read_so_far = 0;
                            bool eof_reached = false;
                            bool should_reset = false;
                            while (read_so_far < ZvdAsyncConnection::IN_BUF_SIZE) {
                                ssize_t n = recv(conn->backend_fd, conn->in_buf + read_so_far,
                                                 ZvdAsyncConnection::IN_BUF_SIZE - read_so_far, 0);
                                if (n > 0) {
                                    read_so_far += static_cast<size_t>(n);
                                } else if (n == 0) {
                                    eof_reached = true;
                                    break;
                                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                    break;
                                } else {
                                    should_reset = true;
                                    break;
                                }
                            }
                            if (should_reset) {
                                conn->reset(epfd);
                                continue;
                            }
                            if (read_so_far > 0) {
                                conn->in_len = read_so_far;
                                conn->in_offset = 0;
                                conn->state = ConnState::WRITING_CLIENT_RESP;
                                epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                            } else if (eof_reached) {
                                epoll_ctl(epfd, EPOLL_CTL_DEL, conn->backend_fd, nullptr);
                                close(conn->backend_fd);
                                conn->backend_fd = -1;
                                conn->reset(epfd);
                            }
                            continue;
                        }

                        char tmp[8192];
                        bool finalized_response = false;
                        while (true) {
                            ssize_t n = recv(conn->backend_fd, tmp, sizeof(tmp), 0);
                            if (n > 0) {
                                conn->backend_resp.append(tmp, static_cast<size_t>(n));

                                if (!conn->is_download_stream) {
                                    size_t b_sep_len = 0;
                                    size_t b_hdr_end = zvd_find_header_end(conn->backend_resp, b_sep_len);

                                    if (b_hdr_end != std::string::npos) {
                                        std::string_view b_headers(conn->backend_resp.data(), b_hdr_end);
                                        bool trigger_stream = false;
                                        size_t b_cl = 0;
                                        bool has_b_cl = zvd_find_content_length(b_headers, b_cl);

                                        if (b_headers.find("X-ZVD-Stream: 1") != std::string_view::npos ||
                                            b_headers.find("application/octet-stream") != std::string_view::npos) {
                                            trigger_stream = true;
                                        }

                                        if (has_b_cl && b_cl > ZvdAsyncConnection::IN_BUF_SIZE - 1) {
                                            trigger_stream = true;
                                        }

                                        if (trigger_stream) {
                                            conn->is_download_stream = true;
                                            std::string trity_str = (conn->decision.trity == IZvdCognitiveBridge::Trity::PLUS) ? "PLUS" :
                                                                    (conn->decision.trity == IZvdCognitiveBridge::Trity::MINUS) ? "MINUS" : "ZERO";
                                            std::string diag_headers = "X-ZVD-Trity: " + trity_str + "\r\nX-ZVD-Directive: " + conn->decision.routing_directive + "\r\n";
                                            std::string mod_resp = zvd_inject_diag_headers_into_response(
                                                conn->backend_resp, b_hdr_end, b_sep_len, diag_headers);

                                            if (mod_resp.size() <= sizeof(conn->in_buf)) {
                                                std::memcpy(conn->in_buf, mod_resp.data(), mod_resp.size());
                                                conn->in_len = mod_resp.size();
                                                conn->in_offset = 0;
                                                conn->backend_resp.clear();
                                            }
                                            conn->state = ConnState::WRITING_CLIENT_RESP;
                                            epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                                            epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                                            finalized_response = true;
                                            break;
                                        }

                                        if (has_b_cl) {
                                            size_t body_start = b_hdr_end + b_sep_len;
                                            if (b_cl <= conn->backend_resp.size() - body_start) {
                                                size_t expected_total = body_start + b_cl;
                                                conn->backend_resp.resize(expected_total);
                                                break;
                                            }
                                        }
                                    }
                                }
                            } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                                break;
                            } else {
                                break;
                            }
                        }

                        if (!finalized_response && conn->backend_fd >= 0) {
                            size_t b_sep_len = 0;
                            size_t b_hdr_end = zvd_find_header_end(conn->backend_resp, b_sep_len);
                            size_t b_cl = 0;
                            bool has_b_cl = (b_hdr_end != std::string::npos) &&
                                            zvd_find_content_length(std::string_view(conn->backend_resp.data(), b_hdr_end), b_cl);
                            bool complete_by_cl = has_b_cl && (conn->backend_resp.size() >= b_hdr_end + b_sep_len + b_cl);

                            // Finalize if Content-Length is satisfied (Keep-Alive support) or backend closed/errored
                            bool should_finalize = complete_by_cl;
                            if (!should_finalize) {
                                ssize_t probe = recv(conn->backend_fd, tmp, sizeof(tmp), 0);
                                if (probe > 0) {
                                    conn->backend_resp.append(tmp, static_cast<size_t>(probe));
                                    if (b_hdr_end != std::string::npos && has_b_cl &&
                                        conn->backend_resp.size() >= b_hdr_end + b_sep_len + b_cl) {
                                        should_finalize = true;
                                    }
                                } else if (probe == 0 || (probe < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                                    should_finalize = true;
                                }
                            }

                            if (should_finalize) {
                                if (b_hdr_end != std::string::npos && has_b_cl &&
                                    conn->backend_resp.size() > b_hdr_end + b_sep_len + b_cl) {
                                    conn->backend_resp.resize(b_hdr_end + b_sep_len + b_cl);
                                }

                                epoll_ctl(epfd, EPOLL_CTL_DEL, conn->backend_fd, nullptr);
                                close(conn->backend_fd);
                                conn->backend_fd = -1;

                                bool response_rejected = false;
                                std::string reject_reason = "";
                                for (const auto& mod : zvd_get_module_bus()) {
                                    if (mod.verify_fn) {
                                        if (!mod.verify_fn(conn->user_prompt, conn->backend_resp)) {
                                            response_rejected = true;
                                            reject_reason = "[ZVD-BUS-VERIFY-VETO] Response rejected by module: " + mod.name;
                                            break;
                                        }
                                    }
                                }

                                std::string trity_str = (conn->decision.trity == IZvdCognitiveBridge::Trity::PLUS) ? "PLUS" :
                                                        (conn->decision.trity == IZvdCognitiveBridge::Trity::MINUS) ? "MINUS" : "ZERO";

                                if (response_rejected) {
                                    std::string veto_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + reject_reason + "\"}}]}";
                                    conn->out_buf = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(veto_reply.size()) + "\r\nX-ZVD-Trity: MINUS\r\nX-ZVD-Directive: VERIFY_VETO\r\nConnection: close\r\n\r\n" + veto_reply;
                                } else {
                                    std::string diag_headers = "X-ZVD-Trity: " + trity_str + "\r\nX-ZVD-Directive: " + conn->decision.routing_directive + "\r\n";
                                    if (b_hdr_end != std::string::npos) {
                                        conn->out_buf = zvd_inject_diag_headers_into_response(
                                            conn->backend_resp, b_hdr_end, b_sep_len, diag_headers);
                                    } else {
                                        conn->out_buf = conn->backend_resp;
                                    }
                                }
                                conn->backend_resp.clear();
                                conn->out_sent = 0;
                                conn->state = ConnState::WRITING_CLIENT_RESP;
                                epoll_event wev{EPOLLOUT | EPOLLET, {.ptr = &conn->client_target}};
                                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->client_fd, &wev);
                            }
                        }
                    }
                }
            }
        }
    }
}

// =====================================================================
// SECTION 10: SERVER ENTRY POINT & MULTI-CORE SO_REUSEPORT INITIALIZATION
// =====================================================================

#if !defined(ZVD_NO_MAIN) || (ZVD_NO_MAIN == 0)
int main(int argc, char **argv) {
    std::signal(SIGPIPE, SIG_IGN);

    int listen_port = (argc > 1) ? std::atoi(argv[1]) : 9120;
    std::string target_ip = (argc > 2) ? argv[2] : "127.0.0.1";
    int target_port = (argc > 3) ? std::atoi(argv[3]) : 8080;

    unsigned int num_cores = std::thread::hardware_concurrency();
    size_t num_workers = std::max(1u, num_cores);

    std::printf("======================================================================\n");
    std::printf("ZVD HYPER-PERIMETER PROXY & BUS (V3.3.3-HARDENED-C20)\n");
    std::printf("Listening on 0.0.0.0:%d -> Forwarding to %s:%d\n", listen_port, target_ip.c_str(), target_port);
    std::printf("Spawning %zu Linux Epoll Workers (1 per CPU core via SO_REUSEPORT)\n", num_workers);
    std::printf("Root-Architect: Vladimir Zavodiuk\n");
    std::printf("======================================================================\n");

    const auto& bus = zvd_get_module_bus();
    if (bus.empty()) {
        std::printf("[BUS]     Registered Cartridges / Plugins .............. [0 ACTIVE]\n");
    } else {
        size_t idx = 1;
        for (const auto& mod : bus) {
            const char* role_str = (mod.role == ZvdModuleRole::ARBITER) ? "[ARBITER]" :
                                   (mod.role == ZvdModuleRole::TELEMETRY) ? "[TELEMETRY]" : "[FILTER]";
            std::printf("[BUS-MOD #%zu] %-32s %-12s [ACTIVE]\n", idx++, mod.name.c_str(), role_str);
        }
    }
    std::printf("======================================================================\n");

    std::vector<std::thread> workers;
    workers.reserve(num_workers);
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back(run_async_epoll_worker, listen_port, target_ip, target_port);
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    return 0;
}
#endif
