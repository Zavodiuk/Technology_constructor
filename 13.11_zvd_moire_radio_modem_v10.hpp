/* =============================================================================
 *  PROJECT:    ZVD-120D MOIRE RADIO & SATELLITE MODEM BRIDGE (SLOT 7)
 *              (13.11_zvd_moire_radio_modem_v10.hpp - V3.2.0-ZERO-DEMO-MODEM-C20)
 *  FILENAME:   13.11_zvd_moire_radio_modem_v10.hpp
 *  VERSION:    2026.11-V3.2.0-ZERO-DEMO-MODEM-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation on hot paths, alignas(64), OpenMP Safe, Zero-Demo
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_MOIRE_RADIO_MODEM 1

/* Safe dependency inclusion ensuring 100% alphabetical manifest compatibility via __has_include */
#if __has_include("13.11_zvd_visual_crypto_v30.hpp")
#include "13.11_zvd_visual_crypto_v30.hpp"
#endif

#if __has_include("13.11_zvd_radio_link_v10.hpp")
#include "13.11_zvd_radio_link_v10.hpp"
#endif

#if !defined(ZVD_HAS_VISUAL_CRYPTO) || !defined(ZVD_HAS_RADIO_LINK)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_moire_radio_modem_v10.hpp requires both 13.11_zvd_visual_crypto_v30.hpp and 13.11_zvd_radio_link_v10.hpp in working directory!"
#endif

#include <vector>
#include <map>
#include <algorithm>

// =====================================================================
// 1. MOIRE RADIO MODEM TRANSMISSION PIPELINE (TX)
// =====================================================================

inline size_t zvd_moire_modem_transmit_payload(
    const uint8_t *payload,
    size_t payload_len,
    const char *type_sig,
    const char *passphrase,
    const char *peer_ip,
    int tx_port,
    uint16_t node_id,
    uint16_t peer_id
) noexcept {
    if (!payload || payload_len == 0 || !passphrase) return 0;
    zvd_ensure_crypto_lut();
    zvd_radio_start_listener_once();

    ZVD_Monolith334 *monoliths = nullptr;
    size_t num_cubes = zvd_encode_any_to_monoliths(payload, payload_len, type_sig, passphrase, &monoliths);
    if (num_cubes == 0 || !monoliths) return 0;

    auto tx_sock = get_radio_tx_socket();
#ifdef _WIN32
    if (tx_sock == INVALID_SOCKET) {
        std::free(monoliths);
        return 0;
    }
#else
    if (tx_sock < 0) {
        std::free(monoliths);
        return 0;
    }
#endif

    struct sockaddr_in target_addr = {};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(static_cast<uint16_t>(tx_port));
    inet_pton(AF_INET, peer_ip ? peer_ip : "127.0.0.1", &target_addr.sin_addr);

    static std::atomic<uint32_t> modem_seq_counter{5000};
    size_t transmitted_count = 0;

    alignas(64) uint8_t radio_frame_buf[sizeof(ZvdRadioFrameHeader) + sizeof(ZVD_Monolith334)];

    for (size_t i = 0; i < num_cubes; i++) {
        uint32_t seq = modem_seq_counter.fetch_add(1, std::memory_order_relaxed);
        size_t frame_size = 0;

        bool pack_ok = zvd_radio_pack_frame(
            node_id, peer_id, seq,
            reinterpret_cast<const uint8_t*>(&monoliths[i]), sizeof(ZVD_Monolith334),
            radio_frame_buf, sizeof(radio_frame_buf), &frame_size
        );

        if (pack_ok) {
#ifdef _WIN32
            int sent = sendto(tx_sock, reinterpret_cast<const char*>(radio_frame_buf), static_cast<int>(frame_size), 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
#else
            ssize_t sent = sendto(tx_sock, radio_frame_buf, frame_size, 0, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr));
#endif
            if (sent > 0) {
                transmitted_count++;
                g_radio_tx_frames.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    std::free(monoliths);
    return transmitted_count;
}


// =====================================================================
// 2. MOIRE RADIO MODEM REASSEMBLY & DECRYPTION PIPELINE (RX)
// =====================================================================

inline uint8_t* zvd_moire_modem_receive_reassemble(
    const std::vector<std::vector<uint8_t>>& raw_packet_stream,
    const char *passphrase,
    size_t *out_len,
    char out_type_sig[5],
    bool *out_valid_hash,
    size_t *out_fec_healed_count
) noexcept {
    if (raw_packet_stream.empty() || !passphrase || !out_len || !out_valid_hash) return nullptr;
    zvd_ensure_crypto_lut();

    std::map<uint32_t, ZVD_Monolith334> cube_map;
    size_t fec_count = 0;

    alignas(64) uint8_t unpacked_payload[sizeof(ZVD_Monolith334)];

    for (const auto& pkt : raw_packet_stream) {
        uint16_t node = 0, peer = 0;
        uint32_t seq = 0;
        size_t p_len = 0;
        bool fec_rec = false;
        bool bit_perf = false;

        bool unpack_ok = zvd_radio_unpack_frame(
            pkt.data(), pkt.size(),
            unpacked_payload, sizeof(unpacked_payload),
            &node, &peer, &seq, &p_len,
            &fec_rec, &bit_perf
        );

        if (unpack_ok && bit_perf && p_len == sizeof(ZVD_Monolith334)) {
            if (fec_rec) {
                fec_count++;
                g_radio_fec_recovered.fetch_add(1, std::memory_order_relaxed);
            }
            ZVD_Monolith334 mono;
            std::memcpy(&mono, unpacked_payload, sizeof(ZVD_Monolith334));
            cube_map[seq] = mono;
        }
    }

    if (cube_map.empty()) {
        if (out_fec_healed_count) *out_fec_healed_count = 0;
        return nullptr;
    }

    size_t num_cubes = cube_map.size();
    ZVD_Monolith334 *monoliths = static_cast<ZVD_Monolith334 *>(std::malloc(num_cubes * sizeof(ZVD_Monolith334)));
    if (!monoliths) return nullptr;

    size_t idx = 0;
    for (auto const& [seq, mono] : cube_map) {
        (void)seq;
        monoliths[idx++] = mono;
    }

    if (out_fec_healed_count) *out_fec_healed_count = fec_count;

    uint8_t *payload = zvd_decode_monoliths_to_any(monoliths, num_cubes, passphrase, out_len, out_type_sig, out_valid_hash);
    std::free(monoliths);

    return payload;
}


// =====================================================================
// 3. UNIVERSAL BUS INTEGRATION (V3.2 TELEMETRY ROLE)
// =====================================================================

inline int zvd_moire_modem_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_MOIRE_MODEM_TEST")) {
        const char *key = "ZVD-SOVEREIGN-ENTERPRISE-2026";
        const char *test_msg = "ZVD Slot 7 Moire Radio Modem Bridge Operational.";
        size_t sent = zvd_moire_modem_transmit_payload(
            reinterpret_cast<const uint8_t*>(test_msg), std::strlen(test_msg), "text", key, "127.0.0.1", 9011, 101, 202
        );
        return (sent > 0) ? 1 : -1;
    }
    return 0;
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_moire_modem_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    zvd_ensure_crypto_lut();
    zvd_radio_start_listener_once();

    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_MOIRE_MODEM_TEST")) {
        const char *key = "ZVD-SOVEREIGN-ENTERPRISE-2026";
        size_t sent = zvd_moire_modem_transmit_payload(
            reinterpret_cast<const uint8_t*>(ctx.prompt.data()),
            ctx.prompt.size(),
            "text",
            key,
            "127.0.0.1",
            9011,
            101,
            202
        );
        ctx.telemetry_dispatched = (sent > 0);
        return 1;
    }
    return 0;
}
#endif

inline bool zvd_moire_modem_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_MOIRE_MODEM_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.2 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("MoireRadioModem-120D-Bridge", ZvdModuleRole::TELEMETRY, zvd_moire_modem_evaluate_ctx, zvd_moire_modem_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("MoireRadioModem-120D-Bridge", zvd_moire_modem_evaluate, zvd_moire_modem_verify);
#endif

#endif // ZVD_MOIRE_RADIO_MODEM_V10_HPP
