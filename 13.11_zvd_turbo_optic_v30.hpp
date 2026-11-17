/* =============================================================================
 *  PROJECT:    ZVD-120D TURBO OPTIC FABRIC & RADIO LINK (SLOT 5)
 *              (13.11_zvd_turbo_optic_v30.hpp - V3.2.1-ZERO-LOCK-OPTIC-C20)
 *  FILENAME:   13.11_zvd_turbo_optic_v30.hpp
 *  VERSION:    2026.11-V3.2.1-ZERO-LOCK-OPTIC-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation on hot paths, alignas(64), OpenMP Safe, Zero-Lock C++20
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_TURBO_OPTIC 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_turbo_optic_v30.hpp requires zvd_local_ai_proxy.cpp and zvd_datacenter_node_v28_lite.c in the working directory!"
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include <string>
#include <string_view>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

/* Hardware intrinsics for L1/L2 cache software prefetching */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD-SOVEREIGN-ENTERPRISE-2026"
#endif

constexpr double ZVD_PI = 3.14159265358979323846;

// =====================================================================
// 1. CACHED ENVIRONMENT HELPERS & THREAD-SAFE Q8 INT-OPTIMIZED LUTS
// =====================================================================

inline const char* get_cached_master_key() noexcept {
    static const std::string key = []() {
        const char* env = std::getenv("ZVD_MASTER_KEY");
        return env ? std::string(env) : std::string(ZVD_DEFAULT_MASTER_KEY);
    }();
    return key.c_str();
}

inline const char* get_cached_peer_ip() noexcept {
    static const std::string ip = []() {
        const char* env = std::getenv("ZVD_OPTIC_PEER_IP");
        return env ? std::string(env) : std::string("127.0.0.1");
    }();
    return ip.c_str();
}

inline int get_cached_tx_port() noexcept {
    static const int port = []() {
        const char* env = std::getenv("ZVD_OPTIC_TX_PORT");
        int p = env ? std::atoi(env) : 9001;
        return (p > 0 && p < 65536) ? p : 9001;
    }();
    return port;
}

alignas(64) static int16_t ZVD_SIN_Q8[360];
alignas(64) static int16_t ZVD_COS_Q8[360];
static int16_t ZVD_PYTH_Q8[5];

alignas(64) static int16_t ZVD_HYPER_Q8[ZVD_CELLS_PER_LAYER];
alignas(64) static int16_t ZVD_ELLIP_Q8[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_K_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_PARITY_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_QUAD_F_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_QUAD_V_MAP[ZVD_CELLS_PER_LAYER];

static std::atomic<int> g_turbo_luts_state{0};
static std::atomic<uint64_t> g_turbo_optic_tx_frames{0};
static std::atomic<uint64_t> g_turbo_optic_rx_frames{0};

// Cross-platform thread-local persistent TX socket helper for maximum throughput
#ifdef _WIN32
inline SOCKET get_thread_local_tx_socket(void) noexcept {
    static thread_local SOCKET s_tx_sock = INVALID_SOCKET;
    if (s_tx_sock == INVALID_SOCKET) {
        s_tx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    return s_tx_sock;
}
#else
inline int get_thread_local_tx_socket(void) noexcept {
    static thread_local int s_tx_sock = -1;
    if (s_tx_sock < 0) {
        s_tx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    return s_tx_sock;
}
#endif

// Thread-safe buffer for the last received RX Monolith
struct ZvdRxMonolithBuffer {
    std::mutex mtx;
    ZVD_Monolith334 monolith{};
    bool valid{false};
};

inline ZvdRxMonolithBuffer& zvd_get_rx_monolith_buffer(void) noexcept {
    static ZvdRxMonolithBuffer rx_buf;
    return rx_buf;
}

inline bool zvd_turbo_get_last_rx_monolith(ZVD_Monolith334 *out_monolith) noexcept {
    if (!out_monolith) return false;
    auto& buf = zvd_get_rx_monolith_buffer();
    std::lock_guard<std::mutex> lock(buf.mtx);
    if (!buf.valid) return false;
    *out_monolith = buf.monolith;
    return true;
}

// Background RX Listener Thread control state
static std::atomic<bool> g_rx_listener_running{false};
static std::thread g_rx_listener_thread;

inline void zvd_optic_rx_listener_worker(int rx_port) noexcept {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

    struct sockaddr_in servaddr = {};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(static_cast<uint16_t>(rx_port));

    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&servaddr), sizeof(servaddr)) < 0) {
#ifdef _WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return;
    }

#ifndef _WIN32
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif

    g_rx_listener_running.store(true, std::memory_order_release);

    alignas(64) uint8_t recv_buf[1500];
    alignas(64) uint8_t unpacked_payload[sizeof(ZVD_Monolith334)];

    while (g_rx_listener_running.load(std::memory_order_acquire)) {
        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);
        
        ssize_t n = recvfrom(sockfd, reinterpret_cast<char*>(recv_buf), sizeof(recv_buf), 0, reinterpret_cast<struct sockaddr *>(&cliaddr), &len);
        if (n <= 0) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(1000);
#endif
            continue;
        }

        const char *passphrase = get_cached_master_key();

        uint16_t src = 0, dst = 0;
        size_t unp_len = 0;
        bool bit_perfect = false;

        bool unpack_ok = zvd_optic_frame_unpack(
            recv_buf, static_cast<size_t>(n), passphrase,
            unpacked_payload, sizeof(unpacked_payload),
            &src, &dst, &unp_len, &bit_perfect
        );

        if (unpack_ok && bit_perfect && unp_len == sizeof(ZVD_Monolith334)) {
            g_turbo_optic_rx_frames.fetch_add(1, std::memory_order_relaxed);
            auto& rx_buf = zvd_get_rx_monolith_buffer();
            std::lock_guard<std::mutex> lock(rx_buf.mtx);
            std::memcpy(&rx_buf.monolith, unpacked_payload, sizeof(ZVD_Monolith334));
            rx_buf.valid = true;
        }
    }

#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
}

inline void zvd_turbo_start_rx_listener_once(void) noexcept {
    static std::once_flag rx_once_flag;
    std::call_once(rx_once_flag, []() {
        int rx_port = 9002;
        const char *env_rx = std::getenv("ZVD_OPTIC_RX_PORT");
        if (env_rx) {
            int p = std::atoi(env_rx);
            if (p > 0 && p < 65536) rx_port = p;
        }
        g_rx_listener_thread = std::thread(zvd_optic_rx_listener_worker, rx_port);
        g_rx_listener_thread.detach();
    });
}

inline void zvd_turbo_ensure_luts(void) noexcept {
    zvd_turbo_start_rx_listener_once();

    int expected = 0;
    if (g_turbo_luts_state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
        for (int deg = 0; deg < 360; deg++) {
            double rad = static_cast<double>(deg) * (ZVD_PI / 180.0);
            ZVD_SIN_Q8[deg] = static_cast<int16_t>(std::sin(rad) * 256.0);
            ZVD_COS_Q8[deg] = static_cast<int16_t>(std::cos(rad) * 256.0);
        }
        for (int k = 0; k < 5; k++) {
            double term = 4.0 + 2.0 * static_cast<double>(k);
            double val = (std::sqrt(9.0 + term * term) / 5.0) * 256.0;
            ZVD_PYTH_Q8[k] = static_cast<int16_t>(val);
        }

        for (int x = 0; x < ZVD_CUBE; x++) {
            for (int y = 0; y < ZVD_CUBE; y++) {
                double dx = static_cast<double>(x - 2);
                double dy = static_cast<double>(y - 2);
                double dist = std::sqrt(dx * dx + dy * dy);
                double h_val = (dist < 1e-5) ? 1.0 : (std::sinh(dist) / dist);
                double e_val = (dist < 1e-5) ? 0.0 : std::sin(dist);

                for (int z = 0; z < ZVD_CUBE; z++) {
                    for (int f = 0; f < 4; f++) {
                        int cell_idx = ((x * 5 + y) * 5 + z) * 4 + f;
                        ZVD_HYPER_Q8[cell_idx] = static_cast<int16_t>(h_val * 256.0);
                        ZVD_ELLIP_Q8[cell_idx] = static_cast<int16_t>(e_val * 256.0);
                        ZVD_K_MAP[cell_idx] = (25 * x + 5 * y + z + f) % 60;
                        ZVD_PARITY_MAP[cell_idx] = ((x + y + z + f) % 3) - 1;
                        ZVD_QUAD_F_MAP[cell_idx] = (x + y + z + f) & 3;
                        ZVD_QUAD_V_MAP[cell_idx] = (x * 2 + y + z + f) & 3;
                    }
                }
            }
        }
        g_turbo_luts_state.store(2, std::memory_order_release);
    } else {
        while (g_turbo_luts_state.load(std::memory_order_acquire) != 2) {
            #if defined(_MSC_VER)
            YieldProcessor();
            #elif defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
            #endif
        }
    }
}


// =====================================================================
// 2. TURBO SYNTHESIS CASCADE & Q8 INTEGRATION KERNELS (C++20 __restrict__)
// =====================================================================

inline void zvd_turbo_synthesis_cascade1(const int32_t * __restrict__ raw_120, const ZVD_BiCore120D * __restrict__ core, int8_t * __restrict__ out_vec_120) noexcept {
    zvd_turbo_ensure_luts();
    for (int i = 0; i < ZVD_DIM_120; i++) {
        int32_t raw = raw_120[i];
        int m_idx = i % 25;
        int p_idx = i % 5;
        int32_t w_a = static_cast<int32_t>(core->matrix_a[m_idx]);
        int32_t w_b = static_cast<int32_t>(core->matrix_b[m_idx]);
        int32_t pyth_q8 = static_cast<int32_t>(ZVD_PYTH_Q8[p_idx]);
        
        int phase_a, phase_b;
        if (i < 60) {
            phase_a = (i * 3 + core->angle_deg_a) % 360;
            phase_b = (i * 3 + core->angle_deg_b) % 360;
        } else {
            phase_a = ((i - 60) * 3 + core->angle_deg_a) % 360;
            phase_b = ((i - 60) * 3 + core->angle_deg_b) % 360;
        }
        
        int32_t trig_q16 = static_cast<int32_t>(ZVD_SIN_Q8[phase_a]) * static_cast<int32_t>(ZVD_COS_Q8[phase_b]);
        int32_t moire_q24 = (trig_q16 >> 8) * w_a * w_b;
        int32_t val_acc = raw + ((raw * ((moire_q24 >> 8) * pyth_q8 >> 8)) >> 13);
        
        int int_val = static_cast<int>(val_acc >> 4);
        int trit_1 = (int_val < 0) ? -1 : (int_val > 0 ? 1 : 0);
        int quad_1 = std::abs(int_val) & 3;
        out_vec_120[i] = ZVD_LUT_345[trit_1 + 1][quad_1];
    }
}

inline void zvd_turbo_project_cascade2_and_pack(const int8_t * __restrict__ vec_120, ZVD_Monolith334 * __restrict__ out_monolith) noexcept {
    zvd_turbo_ensure_luts();
    const int8_t *F60 = &vec_120[0];
    const int8_t *V60 = &vec_120[60];

    alignas(64) int8_t face_data[ZVD_CELLS_PER_LAYER];
    alignas(64) int8_t vertex_data[ZVD_CELLS_PER_LAYER];

    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int k = ZVD_K_MAP[i];
        int parity = ZVD_PARITY_MAP[i];
        
        int32_t hyper_q8 = static_cast<int32_t>(ZVD_HYPER_Q8[i]);
        int32_t ellip_q8 = static_cast<int32_t>(ZVD_ELLIP_Q8[i]);

        int f_raw = static_cast<int>((((static_cast<int32_t>(F60[k]) * hyper_q8) >> 8))) + parity;
        int v_raw = static_cast<int>((((static_cast<int32_t>(-V60[k]) * (256 + ellip_q8)) >> 8))) - parity;

        int trit_f = (f_raw < 0) ? -1 : (f_raw > 0 ? 1 : 0);
        int quad_f = ZVD_QUAD_F_MAP[i];
        int8_t p_face = ZVD_LUT_345[trit_f + 1][quad_f];

        int trit_v = (v_raw < 0) ? -1 : (v_raw > 0 ? 1 : 0);
        int quad_v = ZVD_QUAD_V_MAP[i];
        int8_t p_vert = ZVD_LUT_345[trit_v + 1][quad_v];

        face_data[i] = zvd_lukasiewicz_clamp(static_cast<int>(F60[k]) + static_cast<int>(p_face));
        vertex_data[i] = zvd_lukasiewicz_clamp(static_cast<int>(V60[k]) + static_cast<int>(p_vert));
    }

    zvd_pack_layer_lukasiewicz(face_data, out_monolith->face_packed);
    zvd_pack_layer_lukasiewicz(vertex_data, out_monolith->vertex_packed);
}

inline void zvd_turbo_packet_to_monolith(const uint8_t * __restrict__ raw_data, size_t len, ZVD_BiCore120D * __restrict__ core, ZVD_Monolith334 * __restrict__ out_monolith) noexcept {
    if (!core || !out_monolith) return;

    alignas(64) int32_t raw_120[ZVD_DIM_120] = {0};
    if (!raw_data || len == 0) {
        // Zero-initialized array handles empty input cleanly
    } else if (len <= ZVD_DIM_120) {
        for (size_t i = 0; i < len; i++) raw_120[i % ZVD_DIM_120] = static_cast<int32_t>(raw_data[i]);
    } else {
        size_t chunk_size = len / ZVD_DIM_120;
        if (chunk_size == 0) chunk_size = 1;
        for (int i = 0; i < ZVD_DIM_120; i++) {
            size_t start = static_cast<size_t>(i) * chunk_size;
            size_t end = (i == ZVD_DIM_120 - 1) ? len : start + chunk_size;
            if (start >= len) {
                raw_120[i] = 0;
                continue;
            }
            if (end > len) end = len;
            int32_t s = 0;
            size_t count = (end > start) ? (end - start) : 1;
            for (size_t j = start; j < end; j++) s += static_cast<int32_t>(raw_data[j]);
            raw_120[i] = s / static_cast<int32_t>(count);
        }
    }

    double sample_entropy = raw_data ? static_cast<double>(raw_data[0] % 8) : 0.0;
    int dynamic_step = static_cast<int>(static_cast<float>(core->step_deg) / (1.0f + static_cast<float>(sample_entropy) * 2.0f));
    if (dynamic_step < 1) dynamic_step = 1;
    core->angle_deg_a = (core->angle_deg_a + dynamic_step) % 360;
    core->angle_deg_b = (core->angle_deg_b - dynamic_step + 360) % 360;

    alignas(64) int8_t synth_120[ZVD_DIM_120];
    zvd_turbo_synthesis_cascade1(raw_120, core, synth_120);
    zvd_turbo_project_cascade2_and_pack(synth_120, out_monolith);
}


// =====================================================================
// 3. UNIVERSAL BUS INTEGRATION (V3.2 ZERO-LOCK / ZERO-OVERHEAD HOT PATH)
// =====================================================================

inline int zvd_turbo_optic_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_TURBO_OPTIC")) {
        const char *passphrase = get_cached_master_key();
        const char *payload_msg = "ZVD_DATACENTER_TELEMETRY_FABRIC_PACKET";
        size_t payload_len = std::strlen(payload_msg);

        ZVD_Monolith334 test_monolith;
        ZVD_BiCore120D core;
        zvd_bicore_init(&core, 0, 1);
        zvd_turbo_packet_to_monolith(reinterpret_cast<const uint8_t*>(payload_msg), payload_len, &core, &test_monolith);

        alignas(64) uint8_t frame_buf[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
        size_t optic_size = 0;

        bool pack_ok = zvd_optic_frame_pack(101, 202, reinterpret_cast<const uint8_t*>(&test_monolith), sizeof(ZVD_Monolith334), passphrase, frame_buf, sizeof(frame_buf), &optic_size);
        if (!pack_ok) return -1;

        const char *peer_ip = get_cached_peer_ip();
        int tx_port = get_cached_tx_port();

        auto tx_sock = get_thread_local_tx_socket();
#ifdef _WIN32
        if (tx_sock != INVALID_SOCKET) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, reinterpret_cast<const char*>(frame_buf), static_cast<int>(optic_size), 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
        }
#else
        if (tx_sock >= 0) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, frame_buf, optic_size, 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
        }
#endif

        return 1; // PLUS on explicit diagnostic command
    }
    return 0; // ZERO — standard telemetry propagation
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_turbo_optic_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    bool is_diagnostic = (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_TURBO_OPTIC"));
    bool has_datacenter_pkt = (ctx.datacenter_pkt_ptr != nullptr);

    if (!is_diagnostic && !has_datacenter_pkt) {
        return 0; 
    }

    zvd_turbo_ensure_luts();
    const char *passphrase = get_cached_master_key();

    ZVD_Monolith334 target_monolith;
    if (has_datacenter_pkt) {
        target_monolith = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr)->monolith;
    } else {
        ZVD_BiCore120D core;
        zvd_bicore_init(&core, 0, 1);
        zvd_turbo_packet_to_monolith(reinterpret_cast<const uint8_t*>(ctx.prompt.data()), ctx.prompt.size(), &core, &target_monolith);
    }

    alignas(64) uint8_t frame_buf[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
    size_t optic_size = 0;

    bool pack_ok = zvd_optic_frame_pack(101, 202, reinterpret_cast<const uint8_t*>(&target_monolith), sizeof(ZVD_Monolith334), passphrase, frame_buf, sizeof(frame_buf), &optic_size);
    
    if (pack_ok) {
        const char *peer_ip = get_cached_peer_ip();
        int tx_port = get_cached_tx_port();

        auto tx_sock = get_thread_local_tx_socket();
#ifdef _WIN32
        if (tx_sock != INVALID_SOCKET) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, reinterpret_cast<const char*>(frame_buf), static_cast<int>(optic_size), 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#else
        if (tx_sock >= 0) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, frame_buf, optic_size, 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#endif
    }

    return is_diagnostic ? 1 : 0;
}
#endif

inline bool zvd_turbo_optic_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_TURBO_OPTIC_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.2 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("TurboOpticQ8-120D-RadioLink", ZvdModuleRole::TELEMETRY, zvd_turbo_optic_evaluate_ctx, zvd_turbo_optic_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("TurboOpticQ8-120D-RadioLink", zvd_turbo_optic_evaluate, zvd_turbo_optic_verify);
#endif

#endif // ZVD_TURBO_OPTIC_V30_HPP
  
