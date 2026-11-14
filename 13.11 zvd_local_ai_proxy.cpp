/* =============================================================================
 *  PROJECT:    ZVD SOVEREIGN UNIVERSAL CONSTRUCTOR BUS & COGNITIVE PROXY
 *              (zvd_local_ai_proxy.cpp - V3.1-UNIVERSAL-CONSTRUCTOR-BUS-C20)
 *  FILENAME:   zvd_local_ai_proxy.cpp
 *  VERSION:    2026.11-V3.1-UNIVERSAL-CONSTRUCTOR-BUS-C20
 *
 *  BUILD (Linux/macOS):
 *    g++ -std=c++20 -O3 -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm
 *
 *  UNIVERSAL AUTO-MANIFEST BUILD (Auto-detects all .c and .hpp modules):
 *    python3 -c 'import glob; open("zvd_auto_manifest.h","w").write("#define ZVD_NO_MAIN 1\n" + "\n".join([f"#define ZVD_HAS_DATACENTER_CORE 1\nextern \"C\" {{\n#include \"{f}\"\n}}" for f in glob.glob("*.c")] + [f"#include \"{f}\"" for f in glob.glob("*.hpp")]))' && g++ -std=c++20 -O3 -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  LICENSE:    Proprietary. All rights reserved.
 * =============================================================================
 */

#ifndef ZVD_NO_MAIN
#define ZVD_NO_MAIN 1
#endif

#ifndef restrict
#if defined(__GNUC__) || defined(__clang__)
    #define restrict __restrict__
#elif defined(_MSC_VER)
    #define restrict __restrict
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
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// =====================================================================
// SECTION 1 & 3: UNIFIED PIPELINE CONTEXT & ZVD MODULE BUS
// =====================================================================

struct ZvdPipelineContext {
    std::string_view prompt;
    int8_t gl_invariant = 0;
    double entropy = 0.0;
    const void* datacenter_pkt_ptr = nullptr;
    int accumulated_trity = 0; // -1: MINUS, 0: ZERO, 1: PLUS
    std::string routing_directive = "";
    bool telemetry_dispatched = false;
};

enum class ZvdModuleRole {
    ARBITER,
    TELEMETRY,
    FILTER
};

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
    static ZvdAutoRegistrar _zvd_reg_##__LINE__(NAME, ROLE, nullptr, EVAL_CTX_FN, VERIFY_FN)

// =====================================================================
// SECTION 2: AUTO-MANIFEST INGESTION & FALLBACK DISCOVERY
// =====================================================================

#if defined(__has_include)
  #if __has_include("zvd_auto_manifest.h")
    #define ZVD_HAS_AUTO_MANIFEST 1
    #include "zvd_auto_manifest.h"
  #endif

  #ifndef ZVD_HAS_DATACENTER_CORE
    #if __has_include("13.11 zvd_datacenter_node_v28_lite.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "13.11 zvd_datacenter_node_v28_lite.c"
    #elif __has_include("13.11_zvd_datacenter_node_v28_lite_v2.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "13.11_zvd_datacenter_node_v28_lite_v2.c"
    #elif __has_include("zvd_datacenter_node_v28_lite.c")
      #define ZVD_HAS_DATACENTER_CORE 1
      #define ZVD_CORE_FILENAME "zvd_datacenter_node_v28_lite.c"
    #endif
  #endif

  #if defined(ZVD_HAS_DATACENTER_CORE) && !defined(ZVD_HAS_AUTO_MANIFEST)
    extern "C" {
      #include ZVD_CORE_FILENAME
    }
  #endif

  #if !defined(ZVD_HAS_AUTO_MANIFEST)
    #if __has_include("13 11 zvd_cognitive_theatre.hpp")
      #include "13 11 zvd_cognitive_theatre.hpp"
    #elif __has_include("13.11 zvd_cognitive_theatre.hpp")
      #include "13.11 zvd_cognitive_theatre.hpp"
    #elif __has_include("zvd_cognitive_theatre.hpp")
      #include "zvd_cognitive_theatre.hpp"
    #endif

    #if __has_include("zvd_custom_plugin.hpp")
      #define ZVD_HAS_CUSTOM_PLUGIN 1
      #include "zvd_custom_plugin.hpp"
    #endif
  #endif
#endif

// Stubs for missing optional modules
#ifndef ZVD_HAS_COGNITIVE_AI
inline int zvd_ai_module_evaluate(std::string_view, int8_t, double) { return 0; }
inline bool zvd_ai_module_verify_response(std::string_view, std::string_view) { return true; }
#endif

#ifndef ZVD_HAS_CUSTOM_PLUGIN
inline int zvd_custom_plugin_hook(std::string_view) { return 0; }
#endif

// =====================================================================
// SECTION 4: LOCK-FREE O(1) RING FILTER & OBSERVABILITY COUNTERS
// =====================================================================

static constexpr size_t DEDUP_RING_SIZE = 16384; // 1 << 14
alignas(64) static std::atomic<uint64_t> g_ring_hashes[DEDUP_RING_SIZE] = {0};
alignas(64) static std::atomic<uint32_t> g_ring_counts[DEDUP_RING_SIZE] = {0};

// Observability telemetry atomic counters
alignas(64) static std::atomic<uint64_t> g_total_requests(0);
alignas(64) static std::atomic<uint64_t> g_storm_blocked_count(0);
alignas(64) static std::atomic<uint64_t> g_veto_count(0);

static uint64_t zvd_proxy_fast_hash64(std::string_view sv) {
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

static bool zvd_check_loop_and_dedup(std::string_view prompt) {
    uint64_t h = zvd_proxy_fast_hash64(prompt);
    size_t idx = static_cast<size_t>(h & (DEDUP_RING_SIZE - 1));
    
    uint64_t expected_h = g_ring_hashes[idx].load(std::memory_order_acquire);
    if (expected_h == h) {
        uint32_t cnt = g_ring_counts[idx].fetch_add(1, std::memory_order_relaxed) + 1;
        if (cnt >= 3) {
            return true; // Storm / Loop detected and blocked
        }
        return false;
    }
    
    g_ring_hashes[idx].store(h, std::memory_order_release);
    g_ring_counts[idx].store(1, std::memory_order_release);
    return false;
}

// =====================================================================
// SECTION 3 (Cont.): UNIFIED PIPELINE & ZVD MODULE BUS TRAVERSAL
// =====================================================================

class IZvdCognitiveBridge {
public:
    enum class Trity { PLUS, ZERO, MINUS };

    struct Decision {
        Trity trity;
        std::string action_hint;
        std::string routing_directive;
    };

    static Decision evaluate_prompt(std::string_view prompt) {
        g_total_requests.fetch_add(1, std::memory_order_relaxed);

        // STEP 1: Loop and flood protection check
        if (zvd_check_loop_and_dedup(prompt)) {
            g_storm_blocked_count.fetch_add(1, std::memory_order_relaxed);
            return { Trity::MINUS, "[ZVD-GUARD] Loop detected & blocked. CPU/Battery saved.", "LOOP_BLOCKED" };
        }

        ZvdPipelineContext ctx;
        ctx.prompt = prompt;

        // STEP 2 (SLOT 1): Datacenter Core Ingestion
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
                g_veto_count.fetch_add(1, std::memory_order_relaxed);
                return { Trity::MINUS, "[ZVD-120D-GUARD] High entropy or Godel-Lukasiewicz VETO triggered.", "DATACENTER_VETO" };
            }

            static thread_local int udp_sock = -1;
            if (udp_sock < 0) {
                udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifndef _WIN32
                int flags = fcntl(udp_sock, F_GETFL, 0);
                fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
#endif
            }

            if (udp_sock >= 0) {
                const char* env_port = std::getenv("ZVD_DC_PORT");
                int dc_port = env_port ? std::atoi(env_port) : 9000;
                sockaddr_in dc_addr{};
                dc_addr.sin_family = AF_INET;
                dc_addr.sin_port = htons((uint16_t)dc_port);
                inet_pton(AF_INET, "127.0.0.1", &dc_addr.sin_addr);

                alignas(64) uint8_t optic_frame[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
                size_t optic_sz = 0;
                const char* master_key = std::getenv("ZVD_MASTER_KEY");
                if (!master_key) master_key = ZVD_DEFAULT_MASTER_KEY;

                if (zvd_optic_frame_pack(101, 202, (const uint8_t*)&pkt.monolith, sizeof(ZVD_Monolith334), master_key, optic_frame, sizeof(optic_frame), &optic_sz)) {
                    sendto(udp_sock, (const char*)optic_frame, optic_sz, 0, (struct sockaddr*)&dc_addr, sizeof(dc_addr));
                    ctx.telemetry_dispatched = true;
                }
            }
        }
#endif

        // STEP 3 (UNIVERSAL BUS ZvdModuleBus): Non-short-circuit traversal across all modules
        bool has_plus_verdict = false;
        std::string blocking_module_name = "";

        for (const auto& mod : zvd_get_module_bus()) {
            int verdict = 0;
            if (mod.eval_ctx_fn) {
                verdict = mod.eval_ctx_fn(ctx);
            } else if (mod.eval_fn) {
                verdict = mod.eval_fn(prompt, ctx.gl_invariant, ctx.entropy);
            }

            if (verdict == -1) {
                blocking_module_name = mod.name;
                g_veto_count.fetch_add(1, std::memory_order_relaxed);
                return { Trity::MINUS, "[ZVD-BUS-VETO] Blocked by module: " + mod.name, "BUS_VETO" };
            } else if (verdict == 1) {
                has_plus_verdict = true;
            }
        }

        // STEP 4 (SLOT 3): Custom User Plugin Hook
#if defined(ZVD_HAS_CUSTOM_PLUGIN)
        int plugin_verdict = zvd_custom_plugin_hook(prompt);
        if (plugin_verdict < 0) {
            g_veto_count.fetch_add(1, std::memory_order_relaxed);
            return { Trity::MINUS, "[ZVD-PLUGIN-VETO] Blocked by custom user plugin.", "PLUGIN_VETO" };
        } else if (plugin_verdict > 0) {
            has_plus_verdict = true;
        }
#endif

        if (has_plus_verdict) {
            std::string dir = ctx.routing_directive.empty() ? "BUS_MODULE_BYPASS" : ctx.routing_directive;
            return { Trity::PLUS, dir, dir };
        }

        // STEP 5 (Fallback): Basic prompt heuristics
        if (prompt.size() < 40) {
            return { Trity::PLUS, "FAST_BYPASS", "FAST_CONCISE" };
        }
        return { Trity::ZERO, "DEEP_REASONING_REQUIRED", "DEEP_COT" };
    }
};

// --- Streaming JSON content extractor ---
static std::string extract_json_content(std::string_view body) {
    size_t pos = body.find("\"content\"");
    if (pos == std::string_view::npos) {
        pos = body.find("\"role\":\"user\"");
        if (pos != std::string_view::npos) {
            pos = body.find("\"content\"", pos);
        }
    }
    if (pos == std::string_view::npos) return "";

    size_t colon = body.find(':', pos);
    if (colon == std::string_view::npos) return "";

    size_t first_quote = body.find('"', colon);
    if (first_quote == std::string_view::npos) return "";

    size_t current = first_quote + 1;
    std::string result;
    while (current < body.size()) {
        char c = body[current];
        if (c == '\\' && current + 1 < body.size()) {
            result.push_back(body[current + 1]);
            current += 2;
            continue;
        }
        if (c == '"') break;
        result.push_back(c);
        current++;
    }
    return result;
}

// --- Forward request to local LLM engine with directive injection ---
static std::string forward_to_local_llm(const std::string &target_ip, int target_port, const std::string &raw_request, const std::string &routing_directive) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "HTTP/1.1 502 Bad Gateway\r\n\r\n{\"error\":\"Socket creation failed\"}";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)target_port);
    inet_pton(AF_INET, target_ip.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(sock); WSACleanup();
#else
        close(sock);
#endif
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n{\"error\":\"Cannot connect to local LLM engine\"}";
    }

    std::string final_request = raw_request;
    if (!routing_directive.empty()) {
        size_t header_end = final_request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string custom_header = "X-ZVD-Directive: " + routing_directive + "\r\n";
            final_request.insert(header_end, custom_header);
        }
    }

    send(sock, final_request.data(), final_request.size(), 0);

    std::string response;
    char buffer[4096];
    int bytes = 0;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        response.append(buffer, bytes);
    }

#ifdef _WIN32
    closesocket(sock); WSACleanup();
#else
    close(sock);
#endif

    return response;
}

// =====================================================================
// SECTION 5: SECURE HTTP RECV, METRICS ENDPOINT & BUS VERIFICATION
// =====================================================================

static constexpr size_t MAX_HTTP_BODY_SIZE = 4 * 1024 * 1024; // 4 MB limit

static void handle_client(int client_sock, std::string target_ip, int target_port) {
    std::vector<char> header_buffer;
    header_buffer.reserve(4096);
    char temp_buf[1024];

    size_t header_end_pos = std::string::npos;
    while (true) {
        int received = recv(client_sock, temp_buf, sizeof(temp_buf), 0);
        if (received <= 0) break;
        header_buffer.insert(header_buffer.end(), temp_buf, temp_buf + received);

        if (header_buffer.size() >= 4) {
            for (size_t i = 0; i <= header_buffer.size() - 4; i++) {
                if (header_buffer[i] == '\r' && header_buffer[i+1] == '\n' &&
                    header_buffer[i+2] == '\r' && header_buffer[i+3] == '\n') {
                    header_end_pos = i + 4;
                    break;
                }
            }
        }
        if (header_end_pos != std::string::npos) break;
        if (header_buffer.size() > 65536) break;
    }

    if (header_end_pos == std::string::npos && header_buffer.empty()) {
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        return;
    }

    std::string_view req(header_buffer.data(), header_buffer.size());

    // Intercept GET /zvd/metrics endpoint immediately without LLM forwarding
    if (req.starts_with("GET /zvd/metrics") || req.find("GET /zvd/metrics") == 0) {
        std::ostringstream json_metrics;
        json_metrics << "{\"total_requests\":" << g_total_requests.load()
                     << ",\"storm_blocked_count\":" << g_storm_blocked_count.load()
                     << ",\"veto_triggered_count\":" << g_veto_count.load()
                     << ",\"dedup_ring_size\":" << DEDUP_RING_SIZE
                     << ",\"active_cartridges\":" << zvd_get_module_bus().size() << "}";
        std::string payload = json_metrics.str();
        std::string metrics_http_resp = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: " + std::to_string(payload.size()) + "\r\n"
                                        "Connection: close\r\n\r\n" + payload;
        send(client_sock, metrics_http_resp.data(), metrics_http_resp.size(), 0);
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        return;
    }

    size_t body_offset = (header_end_pos != std::string::npos) ? header_end_pos : 0;

    size_t content_length = 0;
    size_t cl_pos = req.find("Content-Length:");
    if (cl_pos == std::string_view::npos) cl_pos = req.find("content-length:");
    if (cl_pos != std::string_view::npos) {
        size_t colon_p = req.find(':', cl_pos);
        if (colon_p != std::string_view::npos) {
            content_length = std::atoi(req.data() + colon_p + 1);
        }
    }

    std::vector<char> full_request = header_buffer;
    size_t current_body_read = full_request.size() > body_offset ? full_request.size() - body_offset : 0;

    if (content_length > 0 && current_body_read < content_length) {
        if (content_length > MAX_HTTP_BODY_SIZE) {
            std::string err_resp = "HTTP/1.1 413 Payload Too Large\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Payload Too Large\"}";
            send(client_sock, err_resp.data(), err_resp.size(), 0);
#ifdef _WIN32
            closesocket(client_sock);
#else
            close(client_sock);
#endif
            return;
        }

        size_t remaining_to_read = content_length - current_body_read;
        std::vector<char> body_chunk(remaining_to_read);
        size_t total_read = 0;
        while (total_read < remaining_to_read) {
            int r = recv(client_sock, body_chunk.data() + total_read, remaining_to_read - total_read, 0);
            if (r <= 0) break;
            total_read += r;
        }
        full_request.insert(full_request.end(), body_chunk.begin(), body_chunk.begin() + total_read);
    }

    std::string_view final_req(full_request.data(), full_request.size());
    size_t final_body_pos = final_req.find("\r\n\r\n");
    std::string_view body = (final_body_pos != std::string_view::npos) ? final_req.substr(final_body_pos + 4) : "";

    std::string user_prompt = extract_json_content(body);
    auto decision = IZvdCognitiveBridge::evaluate_prompt(user_prompt);

    std::string http_response;
    std::string trity_str = (decision.trity == IZvdCognitiveBridge::Trity::PLUS) ? "PLUS" :
                            (decision.trity == IZvdCognitiveBridge::Trity::MINUS) ? "MINUS" : "ZERO";

    if (decision.trity == IZvdCognitiveBridge::Trity::MINUS) {
        std::fprintf(stderr, "[ZVD-GUARD] %s | Prompt len: %zu\n", decision.action_hint.c_str(), user_prompt.size());
        std::string json_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + decision.action_hint + "\"}}]}";
        http_response = "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: " + std::to_string(json_reply.size()) + "\r\n"
                        "X-ZVD-Trity: " + trity_str + "\r\n"
                        "X-ZVD-Directive: " + decision.routing_directive + "\r\n"
                        "Connection: close\r\n\r\n" + json_reply;
    } else {
        std::string raw_backend_resp = forward_to_local_llm(target_ip, target_port, std::string(final_req), decision.routing_directive);

        bool response_rejected = false;
        std::string reject_reason = "";
        for (const auto& mod : zvd_get_module_bus()) {
            if (mod.verify_fn) {
                if (!mod.verify_fn(user_prompt, raw_backend_resp)) {
                    response_rejected = true;
                    reject_reason = "[ZVD-BUS-VERIFY-VETO] Response rejected by module: " + mod.name;
                    g_veto_count.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }
        }

        if (response_rejected) {
            std::fprintf(stderr, "%s\n", reject_reason.c_str());
            std::string veto_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + reject_reason + "\"}}]}";
            http_response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: " + std::to_string(veto_reply.size()) + "\r\n"
                            "X-ZVD-Trity: MINUS\r\n"
                            "X-ZVD-Directive: VERIFY_VETO\r\n"
                            "Connection: close\r\n\r\n" + veto_reply;
        } else {
            size_t resp_header_end = raw_backend_resp.find("\r\n\r\n");
            if (resp_header_end != std::string::npos) {
                std::string diag_headers = "X-ZVD-Trity: " + trity_str + "\r\n"
                                         + "X-ZVD-Directive: " + decision.routing_directive + "\r\n";
                raw_backend_resp.insert(resp_header_end, diag_headers);
            }
            http_response = raw_backend_resp;
        }
    }

    send(client_sock, http_response.data(), http_response.size(), 0);

#ifdef _WIN32
    closesocket(client_sock);
#else
    close(client_sock);
#endif
}

// =====================================================================
// SECTION 6: STARTUP TELEMETRY AND SERVER ENTRY POINT
// =====================================================================

int main(int argc, char **argv) {
    int listen_port = (argc > 1) ? std::atoi(argv[1]) : 9120;
    std::string target_ip = (argc > 2) ? argv[2] : "127.0.0.1";
    int target_port = (argc > 3) ? std::atoi(argv[3]) : 8080;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::fprintf(stderr, "[!] Failed to create server socket\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)listen_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::fprintf(stderr, "[!] Bind failed on port %d\n", listen_port);
        return 1;
    }

    if (listen(server_sock, 128) < 0) {
        std::fprintf(stderr, "[!] Listen failed\n");
        return 1;
    }

    std::printf("======================================================================\n");
    std::printf("ZVD UNIVERSAL CONSTRUCTOR BUS & COGNITIVE PROXY (V3.1-C20)\n");
    std::printf("Listening on 127.0.0.1:%d -> Forwarding to %s:%d\n", listen_port, target_ip.c_str(), target_port);
    std::printf("Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    std::printf("======================================================================\n");

    std::printf("[ROOT]    ZVD Constructor Bus V3.1 ..................... [ONLINE]\n");
    std::printf("[DEDUP]   Lock-Free O(1) Ring Filter (Size: 16384 slots)  [ACTIVE]\n");
    std::printf("[METRICS] JSON Telemetry Endpoint (/zvd/metrics) ......... [ACTIVE]\n");

#if defined(ZVD_HAS_DATACENTER_CORE)
    std::printf("[SLOT 1]  Datacenter Core 120D & Optic ZVDO ............ [ACTIVE]\n");
#else
    std::printf("[SLOT 1]  Datacenter Core 120D & Optic ZVDO ............ [NOT FOUND]\n");
#endif

    const auto& bus = zvd_get_module_bus();
    if (bus.empty()) {
        std::printf("[BUS]     Registered Cartridges / Plugins .............. [0 ACTIVE]\n");
    } else {
        size_t idx = 1;
        for (const auto& mod : bus) {
            const char* role_str = (mod.role == ZvdModuleRole::ARBITER) ? "[ARBITER]" :
                                   (mod.role == ZvdModuleRole::TELEMETRY) ? "[TELEMETRY]" : "[FILTER]";
            const char* mode_str = mod.eval_ctx_fn ? "[CTX-V3.1]" : "[LEGACY-V3.0]";
            std::printf("[BUS-MOD #%zu] %-32s %-12s %-12s [ACTIVE]\n", idx++, mod.name.c_str(), role_str, mode_str);
        }
    }

    std::printf("======================================================================\n");

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        std::thread(handle_client, client_sock, target_ip, target_port).detach();
    }

#ifdef _WIN32
    closesocket(server_sock);
    WSACleanup();
#else
    close(server_sock);
#endif

    return 0;
}
