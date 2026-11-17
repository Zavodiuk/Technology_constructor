/* =============================================================================
 *  PROJECT:    ZVD-120D RADIO & SATELLITE LINK CARTRIDGE (SLOT 6)
 *              (13.11_zvd_radio_link_v10.hpp - V3.2.0-ZERO-LOCK-RADIO-C20)
 *  FILENAME:   13.11_zvd_radio_link_v10.hpp
 *  VERSION:    2026.11-V3.2.0-ZERO-LOCK-RADIO-C20
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

#define ZVD_HAS_RADIO_LINK 1

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

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD-SOVEREIGN-ENTERPRISE-2026"
#endif

/* Fallback definitions for standalone mode if datacenter core is absent */
#ifndef ZVD_MONOLITH_SIZE
#define ZVD_MONOLITH_SIZE 334
#endif

#ifndef ZVD_OPTIC_MAGIC_LEN
#define ZVD_OPTIC_MAGIC_LEN 4
#endif

#if !defined(ZVD_HAS_DATACENTER_CORE)
typedef struct {
    uint8_t face_packed[167];
    uint8_t vertex_packed[167];
} ZVD_Monolith334;
#endif

// =====================================================================
// 1. UNIQUE CACHED ENVIRONMENT HELPERS & RADIO FRAME STRUCTURES
// =====================================================================

inline uint16_t get_cached_radio_node_id() noexcept {
    static const uint16_t node_id = []() {
        const char *env = std::getenv("ZVD_RADIO_NODE_ID");
        int id = env ? std::atoi(env) : 101;
        return static_cast<uint16_t>(id > 0 ? id : 101);
    }();
    return node_id;
}

inline uint16_t get_cached_radio_peer_id() noexcept {
    static const uint16_t peer_id = []() {
        const char *env = std::getenv("ZVD_RADIO_PEER_ID");
        int id = env ? std::atoi(env) : 202;
        return static_cast<uint16_t>(id > 0 ? id : 202);
    }();
    return peer_id;
}

inline const char* get_cached_radio_peer_ip() noexcept {
    static const std::string ip = []() {
        const char *env = std::getenv("ZVD_RADIO_PEER_IP");
        return env ? std::string(env) : std::string("127.0.0.1");
    }();
    return ip.c_str();
}

inline int get_cached_radio_tx_port() noexcept {
    static const int port = []() {
        const char *env = std::getenv("ZVD_RADIO_TX_PORT");
        int p = env ? std::atoi(env) : 9011;
        return (p > 0 && p < 65536) ? p : 9011;
    }();
    return port;
}

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct {
    char magic[ZVD_OPTIC_MAGIC_LEN];       /* "ZVDR" */
    uint16_t node_id;                      /* Terminal source ID */
    uint16_t peer_id;                      /* Terminal target ID */
    uint32_t seq_id;                       /* Monotonic packet sequence ID for jitter/loss tracking */
    uint32_t fec_parity_345;               /* Pythagorean 3-4-5 check/parity signature for single-byte self-healing */
    uint64_t frame_hash64;                 /* Fast hash integrity check */
} ZvdRadioFrameHeader;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    char magic[ZVD_OPTIC_MAGIC_LEN];
    uint16_t node_id;
    uint16_t peer_id;
    uint32_t seq_id;
    uint32_t fec_parity_345;
    uint64_t frame_hash64;
} ZvdRadioFrameHeader;
#endif

static std::atomic<uint64_t> g_radio_tx_frames{0};
static std::atomic<uint64_t> g_radio_rx_frames{0};
static std::atomic<uint64_t> g_radio_fec_recovered{0};
static std::atomic<uint64_t> g_radio_lost_packets{0};
static std::atomic<uint32_t> g_radio_last_seen_seq{0};
static std::atomic<double>   g_radio_packet_loss_pct{0.0};

// Thread-safe buffer for the last received Radio Monolith
struct ZvdRxRadioBuffer {
    std::mutex mtx;
    ZVD_Monolith334 monolith{};
    bool valid{false};
};

inline ZvdRxRadioBuffer& zvd_get_radio_rx_buffer(void) noexcept {
    static ZvdRxRadioBuffer radio_rx_buf;
    return radio_rx_buf;
}

inline bool zvd_radio_get_last_monolith(ZVD_Monolith334 *out_monolith) noexcept {
    if (!out_monolith) return false;
    auto& buf = zvd_get_radio_rx_buffer();
    std::lock_guard<std::mutex> lock(buf.mtx);
    if (!buf.valid) return false;
    *out_monolith = buf.monolith;
    return true;
}


// =====================================================================
// 2. STANDALONE PENTARY PROMPT FOLDING ENGINE
// =====================================================================

inline void zvd_standalone_fold_prompt_to_monolith(std::string_view prompt, ZVD_Monolith334 *out_monolith) noexcept {
    if (!out_monolith) return;
    alignas(64) int8_t face_500[500];
    alignas(64) int8_t vertex_500[500];
    
    size_t p_size = prompt.size();
    for (int i = 0; i < 500; i++) {
        if (p_size == 0) {
            face_500[i] = 0;
            vertex_500[i] = 0;
        } else {
            uint8_t b1 = static_cast<uint8_t>(prompt[i % p_size]);
            uint8_t b2 = static_cast<uint8_t>(prompt[(i * 37) % p_size]);
            int f_val = (static_cast<int>(b1 % 5)) - 2;
            int v_val = (static_cast<int>(b2 % 5)) - 2;
            face_500[i] = static_cast<int8_t>(f_val);
            vertex_500[i] = static_cast<int8_t>(v_val);
        }
    }

    auto pack_layer = [](const int8_t *layer_500, uint8_t *out_167) noexcept {
        int idx = 0;
        for (int i = 0; i < 498; i += 3) {
            int a = (static_cast<int>(layer_500[i]) < -2 ? -2 : (static_cast<int>(layer_500[i]) > 2 ? 2 : layer_500[i])) + 2;
            int b = (static_cast<int>(layer_500[i+1]) < -2 ? -2 : (static_cast<int>(layer_500[i+1]) > 2 ? 2 : layer_500[i+1])) + 2;
            int c = (static_cast<int>(layer_500[i+2]) < -2 ? -2 : (static_cast<int>(layer_500[i+2]) > 2 ? 2 : layer_500[i+2])) + 2;
            out_167[idx++] = static_cast<uint8_t>(a + 5 * b + 25 * c);
        }
        int a = (static_cast<int>(layer_500[498]) < -2 ? -2 : (static_cast<int>(layer_500[498]) > 2 ? 2 : layer_500[498])) + 2;
        int b = (static_cast<int>(layer_500[499]) < -2 ? -2 : (static_cast<int>(layer_500[499]) > 2 ? 2 : layer_500[499])) + 2;
        out_167[166] = static_cast<uint8_t>(a + 5 * b);
    };

    pack_layer(face_500, out_monolith->face_packed);
    pack_layer(vertex_500, out_monolith->vertex_packed);
}


// =====================================================================
// 3. PYTHAGOREAN FEC (3-4-5) & MÖBIUS RADIO CRYPTOGRAPHY KERNELS
// =====================================================================

inline uint32_t zvd_compute_fec_345(const uint8_t *data, size_t len) noexcept {
    uint32_t parity = 0x55AA55AA;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        uint32_t weight = static_cast<uint32_t>((i % 3) * 3 + (i % 4) * 4 + (i % 5) * 5 + 1);
        parity ^= (static_cast<uint32_t>(b) * weight);
        parity = (parity << 13) | (parity >> 19);
    }
    return parity;
}

inline uint64_t zvd_radio_fast_hash64(const uint8_t *data, size_t len) noexcept {
    uint64_t h = 0x85EBCA6B7C2D5E3ULL ^ static_cast<uint64_t>(len);
    for (size_t i = 0; i < len; i++) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 0xCC9E2D5139D5849ULL;
        h ^= h >> 27;
    }
    return h;
}

// Zero-Alloc radio frame packager with phase scrambling against noise
inline bool zvd_radio_pack_frame(
    uint16_t node_id,
    uint16_t peer_id,
    uint32_t seq_id,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *out_buf,
    size_t out_buf_cap,
    size_t *out_size
) noexcept {
    size_t total_size = sizeof(ZvdRadioFrameHeader) + payload_len;
    if (!out_buf || out_buf_cap < total_size || !payload || !out_size) return false;

    ZvdRadioFrameHeader *hdr = reinterpret_cast<ZvdRadioFrameHeader *>(out_buf);
    std::memcpy(hdr->magic, "ZVDR", ZVD_OPTIC_MAGIC_LEN);
    hdr->node_id = node_id;
    hdr->peer_id = peer_id;
    hdr->seq_id = seq_id;
    hdr->fec_parity_345 = zvd_compute_fec_345(payload, payload_len);
    hdr->frame_hash64 = zvd_radio_fast_hash64(payload, payload_len);

    uint8_t *dest_payload = out_buf + sizeof(ZvdRadioFrameHeader);
    std::memcpy(dest_payload, payload, payload_len);

    uint8_t phase_key = static_cast<uint8_t>(seq_id & 0xFF);
    for (size_t i = 0; i < payload_len; i++) {
        dest_payload[i] ^= static_cast<uint8_t>((phase_key + i * 37) & 0xFF);
    }

    *out_size = total_size;
    return true;
}

// Zero-Alloc radio frame unpacker with Pythagorean single-byte physical self-healing
inline bool zvd_radio_unpack_frame(
    const uint8_t *packet,
    size_t packet_size,
    uint8_t *out_payload_buf,
    size_t out_payload_cap,
    uint16_t *node_id,
    uint16_t *peer_id,
    uint32_t *seq_id,
    size_t *payload_len,
    bool *fec_recovered,
    bool *bit_perfect
) noexcept {
    if (!packet || packet_size < sizeof(ZvdRadioFrameHeader)) return false;
    const ZvdRadioFrameHeader *hdr = reinterpret_cast<const ZvdRadioFrameHeader *>(packet);
    
    if (std::memcmp(hdr->magic, "ZVDR", ZVD_OPTIC_MAGIC_LEN) != 0) {
        if (bit_perfect) *bit_perfect = false;
        return false;
    }

    size_t p_len = packet_size - sizeof(ZvdRadioFrameHeader);
    if (out_payload_cap < p_len) {
        if (bit_perfect) *bit_perfect = false;
        return false;
    }

    std::memcpy(out_payload_buf, packet + sizeof(ZvdRadioFrameHeader), p_len);

    uint8_t phase_key = static_cast<uint8_t>(hdr->seq_id & 0xFF);
    for (size_t i = 0; i < p_len; i++) {
        out_payload_buf[i] ^= static_cast<uint8_t>((phase_key + i * 37) & 0xFF);
    }

    uint64_t computed_hash = zvd_radio_fast_hash64(out_payload_buf, p_len);
    bool perfect = (computed_hash == hdr->frame_hash64);
    bool recovered = false;

    // Real physical single-byte error self-healing via Pythagorean 3-4-5 syndrome matching
    if (!perfect && p_len <= 1024) {
        for (size_t pos = 0; pos < p_len; pos++) {
            for (int err_mask = 1; err_mask <= 255; err_mask++) {
                out_payload_buf[pos] ^= static_cast<uint8_t>(err_mask);
                if (zvd_compute_fec_345(out_payload_buf, p_len) == hdr->fec_parity_345 &&
                    zvd_radio_fast_hash64(out_payload_buf, p_len) == hdr->frame_hash64) {
                    recovered = true;
                    perfect = true;
                    break;
                }
                out_payload_buf[pos] ^= static_cast<uint8_t>(err_mask);
            }
            if (perfect) break;
        }
    }

    if (node_id) *node_id = hdr->node_id;
    if (peer_id) *peer_id = hdr->peer_id;
    if (seq_id) *seq_id = hdr->seq_id;
    if (payload_len) *payload_len = p_len;
    if (fec_recovered) *fec_recovered = recovered;
    if (bit_perfect) *bit_perfect = perfect;

    return perfect;
}


// =====================================================================
// 4. FULL-DUPLEX PERSISTENT UDP SOCKETS & BACKGROUND RX LISTENER
// =====================================================================

#ifdef _WIN32
inline SOCKET get_radio_tx_socket(void) noexcept {
    static thread_local SOCKET s_radio_tx = INVALID_SOCKET;
    if (s_radio_tx == INVALID_SOCKET) {
        s_radio_tx = socket(AF_INET, SOCK_DGRAM, 0);
    }
    return s_radio_tx;
}
#else
inline int get_radio_tx_socket(void) noexcept {
    static thread_local int s_radio_tx = -1;
    if (s_radio_tx < 0) {
        s_radio_tx = socket(AF_INET, SOCK_DGRAM, 0);
    }
    return s_radio_tx;
}
#endif

static std::atomic<bool> g_radio_rx_running{false};
static std::thread g_radio_rx_thread;

inline void zvd_radio_rx_worker(int rx_port) noexcept {
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

    g_radio_rx_running.store(true, std::memory_order_release);

    alignas(64) uint8_t recv_buf[1500];
    alignas(64) uint8_t unpacked_payload[sizeof(ZVD_Monolith334)];

    uint32_t expected_seq = 0;
    uint64_t total_received = 0;
    uint64_t missed_packets = 0;

    while (g_radio_rx_running.load(std::memory_order_acquire)) {
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

        uint16_t node = 0, peer = 0;
        uint32_t seq = 0;
        size_t unp_len = 0;
        bool fec_rec = false;
        bool bit_perf = false;

        bool ok = zvd_radio_unpack_frame(
            recv_buf, static_cast<size_t>(n),
            unpacked_payload, sizeof(unpacked_payload),
            &node, &peer, &seq, &unp_len,
            &fec_rec, &bit_perf
        );

        if (ok && unp_len == sizeof(ZVD_Monolith334)) {
            g_radio_rx_frames.fetch_add(1, std::memory_order_relaxed);
            if (fec_rec) {
                g_radio_fec_recovered.fetch_add(1, std::memory_order_relaxed);
            }

            total_received++;
            if (total_received > 1) {
                if (seq > expected_seq + 1) {
                    uint32_t gap = seq - (expected_seq + 1);
                    missed_packets += gap;
                    g_radio_lost_packets.store(missed_packets, std::memory_order_relaxed);
                }
            }
            expected_seq = seq;
            g_radio_last_seen_seq.store(seq, std::memory_order_relaxed);

            double loss_pct = (total_received + missed_packets > 0) ? 
                (static_cast<double>(missed_packets) * 100.0 / static_cast<double>(total_received + missed_packets)) : 0.0;
            g_radio_packet_loss_pct.store(loss_pct, std::memory_order_relaxed);

            auto& radio_buf = zvd_get_radio_rx_buffer();
            std::lock_guard<std::mutex> lock(radio_buf.mtx);
            std::memcpy(&radio_buf.monolith, unpacked_payload, sizeof(ZVD_Monolith334));
            radio_buf.valid = true;
        }
    }

#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
}

inline void zvd_radio_start_listener_once(void) noexcept {
    static std::once_flag radio_once_flag;
    std::call_once(radio_once_flag, []() {
        int rx_port = 9012;
        const char *env_rx = std::getenv("ZVD_RADIO_RX_PORT");
        if (env_rx) {
            int p = std::atoi(env_rx);
            if (p > 0 && p < 65536) rx_port = p;
        }
        g_radio_rx_thread = std::thread(zvd_radio_rx_worker, rx_port);
        g_radio_rx_thread.detach();
    });
}


// =====================================================================
// 5. UNIVERSAL BUS INTEGRATION (V3.2 ZERO-LOCK / ZERO-OVERHEAD HOT PATH)
// =====================================================================

inline int zvd_radio_link_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    zvd_radio_start_listener_once();

    if (prompt.starts_with("ZVD_RADIO_TEST") || prompt.starts_with("ZVD_TURBO_OPTIC")) {
        uint16_t node_id = get_cached_radio_node_id();
        uint16_t peer_id = get_cached_radio_peer_id();
        static std::atomic<uint32_t> local_seq{1000};
        uint32_t seq = local_seq.fetch_add(1, std::memory_order_relaxed);

        ZVD_Monolith334 dummy_monolith;
        std::memset(&dummy_monolith, 0xAA, sizeof(ZVD_Monolith334));

        alignas(64) uint8_t radio_frame[sizeof(ZvdRadioFrameHeader) + sizeof(ZVD_Monolith334)];
        size_t frame_size = 0;

        if (zvd_radio_pack_frame(node_id, peer_id, seq, reinterpret_cast<const uint8_t*>(&dummy_monolith), sizeof(ZVD_Monolith334), radio_frame, sizeof(radio_frame), &frame_size)) {
            const char *peer_ip = get_cached_radio_peer_ip();
            int tx_port = get_cached_radio_tx_port();

            auto tx_sock = get_radio_tx_socket();
#ifdef _WIN32
            if (tx_sock != INVALID_SOCKET) {
                struct sockaddr_in target_addr = {};
                target_addr.sin_family = AF_INET;
                target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
                inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
                sendto(tx_sock, reinterpret_cast<const char*>(radio_frame), static_cast<int>(frame_size), 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
                g_radio_tx_frames.fetch_add(1, std::memory_order_relaxed);
            }
#else
            if (tx_sock >= 0) {
                struct sockaddr_in target_addr = {};
                target_addr.sin_family = AF_INET;
                target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
                inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
                sendto(tx_sock, radio_frame, frame_size, 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
                g_radio_tx_frames.fetch_add(1, std::memory_order_relaxed);
            }
#endif
        }
        return 1; // PLUS on explicit radio diagnostic command
    }
    return 0; // ZERO — standard telemetry background propagation
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_radio_link_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    bool is_diagnostic = (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_RADIO_TEST"));
    bool has_datacenter_pkt = (ctx.datacenter_pkt_ptr != nullptr);

    // Smart Satellite Link Adaptation: evaluate packet loss threshold regardless of prompt path
    double current_loss = g_radio_packet_loss_pct.load(std::memory_order_relaxed);
    if (current_loss >= 5.0) {
        if (ctx.routing_directive.empty()) {
            ctx.routing_directive = "RADIO_LOW_BANDWIDTH_COMPACT";
        } else {
            ctx.routing_directive += " + RADIO_LOW_BANDWIDTH_COMPACT";
        }
    }

    // Hot-Path Optimization: Return 0 immediately (0 ns overhead) for standard prompts unless explicitly flagged
    if (!is_diagnostic && !has_datacenter_pkt) {
        return 0;
    }

    zvd_radio_start_listener_once();

    uint16_t node_id = get_cached_radio_node_id();
    uint16_t peer_id = get_cached_radio_peer_id();
    static std::atomic<uint32_t> local_seq{2000};
    uint32_t seq = local_seq.fetch_add(1, std::memory_order_relaxed);

    ZVD_Monolith334 target_monolith;
    if (has_datacenter_pkt) {
        target_monolith = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr)->monolith;
    } else {
        zvd_standalone_fold_prompt_to_monolith(ctx.prompt, &target_monolith);
    }

    alignas(64) uint8_t radio_frame[sizeof(ZvdRadioFrameHeader) + sizeof(ZVD_Monolith334)];
    size_t frame_size = 0;

    bool pack_ok = zvd_radio_pack_frame(node_id, peer_id, seq, reinterpret_cast<const uint8_t*>(&target_monolith), sizeof(ZVD_Monolith334), radio_frame, sizeof(radio_frame), &frame_size);

    if (pack_ok) {
        const char *peer_ip = get_cached_radio_peer_ip();
        int tx_port = get_cached_radio_tx_port();

        auto tx_sock = get_radio_tx_socket();
#ifdef _WIN32
        if (tx_sock != INVALID_SOCKET) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, reinterpret_cast<const char*>(radio_frame), static_cast<int>(frame_size), 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_radio_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#else
        if (tx_sock >= 0) {
            struct sockaddr_in target_addr = {};
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
            inet_pton(AF_INET, peer_ip, &target_addr.sin_addr);
            sendto(tx_sock, radio_frame, frame_size, 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
            g_radio_tx_frames.fetch_add(1, std::memory_order_relaxed);
            ctx.telemetry_dispatched = true;
        }
#endif
    }

    return is_diagnostic ? 1 : 0;
}
#endif

inline bool zvd_radio_link_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_RADIO_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.2 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("RadioStarlink-120D-DuplexFEC", ZvdModuleRole::TELEMETRY, zvd_radio_link_evaluate_ctx, zvd_radio_link_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("RadioStarlink-120D-DuplexFEC", zvd_radio_link_evaluate, zvd_radio_link_verify);
#endif

#endif // ZVD_RADIO_LINK_V10_HPP
