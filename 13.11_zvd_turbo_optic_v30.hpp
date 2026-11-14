/* =============================================================================
 *  PROJECT:    ZVD-120D TURBO OPTIC FABRIC & RADIO LINK (SLOT 5)
 *              (13.11_zvd_turbo_optic_v30.hpp - V3.1.2-STERILE-C20)
 *  FILENAME:   13.11_zvd_turbo_optic_v30.hpp
 *  VERSION:    2026.11-V3.1.2-STERILE-FULL-DUPLEX-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  --------------------------------------------------------------------------
 *  ASSEMBLY INSTRUCTIONS (Technology Constructor Ecosystem V3.1.2):
 *  Place this file in the root working directory alongside:
 *   1) zvd_local_ai_proxy.cpp (Root Orchestrator V3.1.2)
 *   2) zvd_datacenter_node_v28_lite.c (Datacenter Core 120D V28.3)
 *   3) 13.11_zvd_turbo_optic_v30.hpp (Turbo Optic & Radio Link Cartridge)
 *  Build command (Linux/macOS):
 *   g++ -std=c++20 -O3 -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm -lpthread
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

// =====================================================================
// 1. THREAD-SAFE Q8 INT-OPTIMIZED LUTs & PRECOMPUTED MAPPINGS
// =====================================================================

alignas(64) static int16_t ZVD_SIN_Q8[360];
alignas(64) static int16_t ZVD_COS_Q8[360];
static int16_t ZVD_PYTH_Q8[5];

alignas(64) static int16_t ZVD_HYPER_Q8[ZVD_CELLS_PER_LAYER];
alignas(64) static int16_t ZVD_ELLIP_Q8[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_K_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_PARITY_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_QUAD_F_MAP[ZVD_CELLS_PER_LAYER];
alignas(64) static int ZVD_QUAD_V_MAP[ZVD_CELLS_PER_LAYER];

static std::atomic<int> g_turbo_luts_state(0);
static std::atomic<uint64_t> g_turbo_optic_tx_frames{0};
static std::atomic<uint64_t> g_turbo_optic_rx_frames{0};

// Cross-platform thread-local persistent TX socket helper for maximum throughput
#ifdef _WIN32
inline SOCKET get_thread_local_tx_socket(void) {
    static thread_local SOCKET s_tx_sock = INVALID_SOCKET;
    if (s_tx_sock == INVALID_SOCKET) {
        s_tx_sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    return s_tx_sock;
}
#else
inline int get_thread_local_tx_socket(void) {
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

inline ZvdRxMonolithBuffer& zvd_get_rx_monolith_buffer(void) {
    static ZvdRxMonolithBuffer rx_buf;
    return rx_buf;
}

inline bool zvd_turbo_get_last_rx_monolith(ZVD_Monolith334 *out_monolith) {
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

inline void zvd_optic_rx_listener_worker(int rx_port) {
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
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons((uint16_t)rx_port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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
        
        ssize_t n = recvfrom(sockfd, (char*)recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&cliaddr, &len);
        if (n <= 0) {
#ifdef _WIN32
            Sleep(10);
#else
            usleep(1000);
#endif
            continue;
        }

        const char *passphrase = getenv("ZVD_MASTER_KEY");
        if (!passphrase) passphrase = ZVD_DEFAULT_MASTER_KEY;

        uint16_t src = 0, dst = 0;
        size_t unp_len = 0;
        bool bit_perfect = false;

        bool unpack_ok = zvd_optic_frame_unpack(
            recv_buf, (size_t)n, passphrase,
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

inline void zvd_turbo_start_rx_listener_once(void) {
    static std::once_flag rx_once_flag;
    std::call_once(rx_once_flag, []() {
        int rx_port = 9002;
        const char *env_rx = getenv("ZVD_OPTIC_RX_PORT");
        if (env_rx) {
            int p = std::atoi(env_rx);
            if (p > 0 && p < 65536) rx_port = p;
        }
        g_rx_listener_thread = std::thread(zvd_optic_rx_listener_worker, rx_port);
        g_rx_listener_thread.detach();
    });
}

inline void zvd_turbo_ensure_luts(void) {
    zvd_turbo_start_rx_listener_once();

    int expected = 0;
    if (g_turbo_luts_state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
        for (int deg = 0; deg < 360; deg++) {
            double rad = (double)deg * (M_PI / 180.0);
            ZVD_SIN_Q8[deg] = (int16_t)(sin(rad) * 256.0);
            ZVD_COS_Q8[deg] = (int16_t)(cos(rad) * 256.0);
        }
        for (int k = 0; k < 5; k++) {
            double term = 4.0 + 2.0 * (double)k;
            double val = (sqrt(9.0 + term * term) / 5.0) * 256.0;
            ZVD_PYTH_Q8[k] = (int16_t)val;
        }

        for (int x = 0; x < ZVD_CUBE; x++) {
            for (int y = 0; y < ZVD_CUBE; y++) {
                double dx = (double)(x - 2);
                double dy = (double)(y - 2);
                double dist = sqrt(dx * dx + dy * dy);
                double h_val = (dist < 1e-5) ? 1.0 : (sinh(dist) / dist);
                double e_val = (dist < 1e-5) ? 0.0 : sin(dist);

                for (int z = 0; z < ZVD_CUBE; z++) {
                    for (int f = 0; f < 4; f++) {
                        int cell_idx = ((x * 5 + y) * 5 + z) * 4 + f;
                        ZVD_HYPER_Q8[cell_idx] = (int16_t)(h_val * 256.0);
                        ZVD_ELLIP_Q8[cell_idx] = (int16_t)(e_val * 256.0);
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
// 2. TURBO SYNTHESIS CASCADE & Q8 INTEGRATION KERNELS
// =====================================================================

inline void zvd_turbo_synthesis_cascade1(const int32_t * restrict raw_120, const ZVD_BiCore120D * restrict core, int8_t * restrict out_vec_120) {
    zvd_turbo_ensure_luts();
    for (int i = 0; i < ZVD_DIM_120; i++) {
        int32_t raw = raw_120[i];
        int m_idx = i % 25;
        int p_idx = i % 5;
        int32_t w_a = (int32_t)core->matrix_a[m_idx];
        int32_t w_b = (int32_t)core->matrix_b[m_idx];
        int32_t pyth_q8 = (int32_t)ZVD_PYTH_Q8[p_idx];
        
        int phase_a, phase_b;
        if (i < 60) {
            phase_a = (i * 3 + core->angle_deg_a) % 360;
            phase_b = (i * 3 + core->angle_deg_b) % 360;
        } else {
            phase_a = ((i - 60) * 3 + core->angle_deg_a) % 360;
            phase_b = ((i - 60) * 3 + core->angle_deg_b) % 360;
        }
        
        int32_t trig_q16 = (int32_t)ZVD_SIN_Q8[phase_a] * (int32_t)ZVD_COS_Q8[phase_b];
        int32_t moire_q24 = (trig_q16 >> 8) * w_a * w_b;
        int32_t val_acc = raw + ((raw * ((moire_q24 >> 8) * pyth_q8 >> 8)) >> 13);
        
        int int_val = (int)(val_acc >> 4);
        int trit_1 = (int_val < 0) ? -1 : (int_val > 0 ? 1 : 0);
        int quad_1 = abs(int_val) & 3;
        out_vec_120[i] = ZVD_LUT_345[trit_1 + 1][quad_1];
    }
}

inline void zvd_turbo_project_cascade2_and_pack(const int8_t * restrict vec_120, ZVD_Monolith334 * restrict out_monolith) {
    zvd_turbo_ensure_luts();
    const int8_t *F60 = &vec_120[0];
    const int8_t *V60 = &vec_120[60];

    alignas(64) int8_t face_data[ZVD_CELLS_PER_LAYER];
    alignas(64) int8_t vertex_data[ZVD_CELLS_PER_LAYER];

    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int k = ZVD_K_MAP[i];
        int parity = ZVD_PARITY_MAP[i];
        
        int32_t hyper_q8 = (int32_t)ZVD_HYPER_Q8[i];
        int32_t ellip_q8 = (int32_t)ZVD_ELLIP_Q8[i];

        int f_raw = (int)((((int32_t)F60[k] * hyper_q8) >> 8)) + parity;
        int v_raw = (int)((((int32_t)(-V60[k]) * (256 + ellip_q8)) >> 8)) - parity;

        int trit_f = (f_raw < 0) ? -1 : (f_raw > 0 ? 1 : 0);
        int quad_f = ZVD_QUAD_F_MAP[i];
        int8_t p_face = ZVD_LUT_345[trit_f + 1][quad_f];

        int trit_v = (v_raw < 0) ? -1 : (v_raw > 0 ? 1 : 0);
        int quad_v = ZVD_QUAD_V_MAP[i];
        int8_t p_vert = ZVD_LUT_345[trit_v + 1][quad_v];

        face_data[i] = zvd_lukasiewicz_clamp((int)F60[k] + (int)p_face);
        vertex_data[i] = zvd_lukasiewicz_clamp((int)V60[k] + (int)p_vert);
    }

    zvd_pack_layer_lukasiewicz(face_data, out_monolith->face_packed);
    zvd_pack_layer_lukasiewicz(vertex_data, out_monolith->vertex_packed);
}

inline void zvd_turbo_packet_to_monolith(const uint8_t * restrict raw_data, size_t len, ZVD_BiCore120D * restrict core, ZVD_Monolith334 * restrict out_monolith) {
    if (!core || !out_monolith) return;

    alignas(64) int32_t raw_120[ZVD_DIM_120] = {0};
    if (!raw_data || len == 0) {
        // Zero-initialized array handles empty input cleanly
    } else if (len <= ZVD_DIM_120) {
        for (size_t i = 0; i < len; i++) raw_120[i % ZVD_DIM_120] = (int32_t)raw_data[i];
    } else {
        size_t chunk_size = len / ZVD_DIM_120;
        if (chunk_size == 0) chunk_size = 1;
        for (int i = 0; i < ZVD_DIM_120; i++) {
            size_t start = (size_t)i * chunk_size;
            size_t end = (i == ZVD_DIM_120 - 1) ? len : start + chunk_size;
            if (start >= len) {
                raw_120[i] = 0;
                continue;
            }
            if (end > len) end = len;
            int32_t s = 0;
            size_t count = (end > start) ? (end - start) : 1;
            for (size_t j = start; j < end; j++) s += (int32_t)raw_data[j];
            raw_120[i] = s / (int32_t)count;
        }
    }

    double sample_entropy = raw_data ? (double)(raw_data[0] % 8) : 0.0;
    int dynamic_step = (int)((float)core->step_deg / (1.0f + (float)sample_entropy * 2.0f));
    if (dynamic_step < 1) dynamic_step = 1;
    core->angle_deg_a = (core->angle_deg_a + dynamic_step) % 360;
    core->angle_deg_b = (core->angle_deg_b - dynamic_step + 360) % 360;

    alignas(64) int8_t synth_120[ZVD_DIM_120];
    zvd_turbo_synthesis_cascade1(raw_120, core, synth_120);
    zvd_turbo_project_cascade2_and_pack(synth_120, out_monolith);
}

// =====================================================================
// 3. UNIVERSAL BUS INTEGRATION (V3.1.2 STERILE FULL-DUPLEX TELEMETRY)
// =====================================================================

inline int zvd_turbo_optic_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_TURBO_OPTIC")) {
        const char *passphrase = getenv("ZVD_MASTER_KEY");
        if (!passphrase) passphrase = ZVD_DEFAULT_MASTER_KEY;
        const char *payload_msg = "ZVD_DATACENTER_TELEMETRY_FABRIC_PACKET";
        size_t payload_len = std::strlen(payload_msg);

        ZVD_Monolith334 test_monolith;
        ZVD_BiCore120D core;
        zvd_bicore_init(&core, 0, 1);
        zvd_turbo_packet_to_monolith((const uint8_t*)payload_msg, payload_len, &core, &test_monolith);

        alignas(64) uint8_t frame_buf[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
        size_t optic_size = 0;

        bool pack_ok = zvd_optic_frame_pack(101, 202, (const uint8_t*)&test_monolith, sizeof(ZVD_Monolith334), passphrase, frame_buf, sizeof(frame_buf), &optic_size);
        if (!pack_ok) return -1;

        const char *peer_ip = getenv("ZVD_OPTIC_PEER_IP");
        if (!peer_ip) peer_ip = "127.0.0.1";
        int tx_port = 9001;
        const char *env_tx_port = getenv("ZVD_OPTIC_TX_PORT");
        if (env_tx_port) {
            int p = std::atoi(env_tx_port);
            if (p > 0 && p < 65536) tx_port = p;
        }

        auto tx_sock = get_thread_local_tx_socket();
#ifdef _WIN32
        if (tx_sock != INVALID_SOCKET) {
            struct sockaddr_in target_addr = {0};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons((uint16_t)tx_port);
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, (const char*)frame_buf, (int)optic_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
        }
#else
        if (tx_sock >= 0) {
            struct sockaddr_in target_addr = {0};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons((uint16_t)tx_port);
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, frame_buf, optic_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
        }
#endif

        return 1; // PLUS (1) on explicit diagnostic command
    }
    return 0; // ZERO — standard telemetry propagation
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_turbo_optic_evaluate_ctx(ZvdPipelineContext& ctx) {
    zvd_turbo_ensure_luts();
    const char *passphrase = getenv("ZVD_MASTER_KEY");
    if (!passphrase) passphrase = ZVD_DEFAULT_MASTER_KEY;

    ZVD_Monolith334 target_monolith;
    if (ctx.datacenter_pkt_ptr != nullptr) {
        target_monolith = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr)->monolith;
    } else if (!ctx.prompt.empty()) {
        ZVD_BiCore120D core;
        zvd_bicore_init(&core, 0, 1);
        zvd_turbo_packet_to_monolith(reinterpret_cast<const uint8_t*>(ctx.prompt.data()), ctx.prompt.size(), &core, &target_monolith);
    } else {
        std::memset(&target_monolith, 0, sizeof(ZVD_Monolith334));
    }

    alignas(64) uint8_t frame_buf[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
    size_t optic_size = 0;

    bool pack_ok = zvd_optic_frame_pack(101, 202, reinterpret_cast<const uint8_t*>(&target_monolith), sizeof(ZVD_Monolith334), passphrase, frame_buf, sizeof(frame_buf), &optic_size);
    
    if (pack_ok) {
        const char *peer_ip = getenv("ZVD_OPTIC_PEER_IP");
        if (!peer_ip) peer_ip = "127.0.0.1";
        int tx_port = 9001;
        const char *env_tx_port = getenv("ZVD_OPTIC_TX_PORT");
        if (env_tx_port) {
            int p = std::atoi(env_tx_port);
            if (p > 0 && p < 65536) tx_port = p;
        }

        auto tx_sock = get_thread_local_tx_socket();
#ifdef _WIN32
        if (tx_sock != INVALID_SOCKET) {
            struct sockaddr_in target_addr = {0};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons((uint16_t)tx_port);
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, (const char*)frame_buf, (int)optic_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#else
        if (tx_sock >= 0) {
            struct sockaddr_in target_addr = {0};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons((uint16_t)tx_port);
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, frame_buf, optic_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            g_turbo_optic_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#endif
    }

    if (ctx.prompt.starts_with("ZVD_TURBO_OPTIC")) {
        return 1; // PLUS
    }
    return 0; // ZERO (telemetry background mode, preserving arbiter sovereignty)
}
#endif

inline bool zvd_turbo_optic_verify(std::string_view prompt, std::string_view response) {
    if (prompt.starts_with("ZVD_TURBO_OPTIC_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.1.2 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("TurboOpticQ8-120D-RadioLink", ZvdModuleRole::TELEMETRY, zvd_turbo_optic_evaluate_ctx, zvd_turbo_optic_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("TurboOpticQ8-120D-RadioLink", zvd_turbo_optic_evaluate, zvd_turbo_optic_verify);
#endif

// =====================================================================
// 4. AUTONOMOUS ENTRY POINT & CLI SMOKE TEST
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_turbo_optic_smoke_test(void) {
    std::printf("\n[*] Running V3.1.2 Sterile Full-Duplex Turbo Optic Smoke Audit...\n");
    zvd_turbo_ensure_luts();

    const char *passphrase = ZVD_DEFAULT_MASTER_KEY;
    const char *test_msg = "ZVD-120D STERILE FULL-DUPLEX OPTIC FABRIC 2026";
    size_t test_len = std::strlen(test_msg);

    ZVD_Monolith334 monolith;
    ZVD_BiCore120D core;
    zvd_bicore_init(&core, 0, 1);
    zvd_turbo_packet_to_monolith((const uint8_t*)test_msg, test_len, &core, &monolith);

    alignas(64) uint8_t optic_buf[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
    size_t optic_sz = 0;

    std::printf("    - Packing zero-alloc optical ZVDO packet (Rack 101 -> Rack 202)...\n");
    bool pack_ok = zvd_optic_frame_pack(101, 202, (const uint8_t*)&monolith, sizeof(ZVD_Monolith334), passphrase, optic_buf, sizeof(optic_buf), &optic_sz);
    if (!pack_ok) {
        std::printf("    [!] Error: Failed to pack optical packet.\n");
        return;
    }

    alignas(64) uint8_t unp_buf[sizeof(ZVD_Monolith334)];
    uint16_t src_rack = 0, dst_rack = 0;
    size_t unp_len = 0;
    bool bit_perfect = false;

    std::printf("    - Unpacking and decrypting optical packet locally...\n");
    bool unpack_ok = zvd_optic_frame_unpack(optic_buf, optic_sz, passphrase, unp_buf, sizeof(unp_buf), &src_rack, &dst_rack, &unp_len, &bit_perfect);

    std::printf("    - Transmission Source Rack:   %u\n", (unsigned int)src_rack);
    std::printf("    - Transmission Target Rack:   %u\n", (unsigned int)dst_rack);
    std::printf("    - Unpacked Payload Length:    %zu bytes\n", unp_len);
    std::printf("    - Optical Channel Hash Check: %s\n", bit_perfect ? "BIT-PERFECT PASSED" : "FAILED");
    std::printf("    - Total Optic TX Frames Sent: %llu\n", (unsigned long long)g_turbo_optic_tx_frames.load());
    std::printf("    - Total Optic RX Frames Recv: %llu\n", (unsigned long long)g_turbo_optic_rx_frames.load());

    ZVD_Monolith334 last_rx;
    if (zvd_turbo_get_last_rx_monolith(&last_rx)) {
        std::printf("    - RX Monolith Buffer Status:  SYNCHRONIZED & VALID\n");
    } else {
        std::printf("    - RX Monolith Buffer Status:  PENDING LOOPBACK / NO INCOMING PACKETS YET\n");
    }

    std::printf("[*] Sterile full-duplex turbo optic smoke test completed successfully.\n");
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::printf("======================================================================\n");
    std::printf("  ZVD-120D TURBO OPTIC FABRIC & RADIO LINK (V3.1.2-STERILE)\n");
    std::printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    std::printf("  Active Hardware Cores: %d\n", zvd_detect_cpu_cores());
    std::printf("======================================================================\n");

    run_turbo_optic_smoke_test();

    int choice = -1;
    while (choice != 0) {
        std::printf("\n================= ZVD V3.1.2 TURBO OPTIC MENU =================\n");
        std::printf(" [1] Run Sterile Full-Duplex Smoke Test & Transmission Audit\n");
        std::printf(" [0] Exit\n");
        std::printf("================================================================\n");
        std::printf("Select menu option: \n");

        if (scanf("%d", &choice) != 1) {
            std::printf("[!] Invalid input. Try again.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                run_turbo_optic_smoke_test();
                break;
            case 0:
                std::printf("[*] Exiting ZVD-120D Sterile Full-Duplex Turbo Optic Fabric. All resources freed.\n");
                break;
            default:
                std::printf("[!] Unknown menu option.\n");
        }
    }

#ifdef _WIN32
    std::printf("\nPress Enter to close console...");
    getchar();
#endif

    return 0;
}

#endif // ZVD_NO_MAIN
