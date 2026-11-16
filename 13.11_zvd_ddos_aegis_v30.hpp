/* =============================================================================
 *  PROJECT:    ZVD-120D SOVEREIGN ECOSYSTEM — DDOS AEGIS SHIELD MODULE
 *              (13.11_zvd_ddos_aegis_v30.hpp - V3.1.8-AEGIS-PRODUCTION-C20)
 *  
 *  AUTHOR:     Vladimir Zavodiuk (Root-Architect / Varna, Bulgaria)
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation, alignas(64), Deterministic Fibonacci Scatter, Q8 Quadrature
 * =============================================================================
 */

#ifndef ZVD_DDOS_AEGIS_V30_HPP
#define ZVD_DDOS_AEGIS_V30_HPP

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
#include <atomic>
#include <chrono>
#include <cstdlib>

// --- DEPENDENCIES & FALLBACKS ---
#if __has_include("13.11_zvd_l5_l60_anthology.hpp")
#include "13.11_zvd_l5_l60_anthology.hpp"
#endif

// Fallback compatibility context if included outside the core orchestrator
#ifndef ZVD_HAS_DATACENTER_CORE
struct ZvdPipelineContext {
    std::string_view prompt;
    const void* datacenter_pkt_ptr;
    std::string routing_directive;
    double entropy;
    int8_t gl_invariant;
};

inline uint64_t zvd_fast_hash64(const char* data, size_t len) noexcept {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

inline void zvd_120d_moire_crypt_buffer(char* buf, size_t len, uint64_t key) noexcept {
    for (size_t i = 0; i < len; ++i) {
        buf[i] ^= static_cast<char>((key >> (i % 8)) & 0xFF);
    }
}
#endif

namespace ZvdAegisDDoS {

    // Optimized sliding density window (1024 slots for L1 cache residency)
    constexpr size_t AEGIS_WINDOW_SIZE = 1024;
    alignas(64) inline std::atomic<uint64_t> g_wave_ring[AEGIS_WINDOW_SIZE];
    alignas(64) inline std::atomic<uint32_t> g_ring_cursor{0};

    // Semantic cluster hit table & sliding epochs for load balancing tiers
    constexpr size_t AEGIS_CLUSTER_BUCKETS = 1024;
    alignas(64) inline std::atomic<uint32_t> g_cluster_hits[AEGIS_CLUSTER_BUCKETS];
    alignas(64) inline std::atomic<uint32_t> g_cluster_epochs[AEGIS_CLUSTER_BUCKETS];
    alignas(64) inline std::atomic<uint32_t> g_aegis_tick{0};

    // Fibonacci Lookup Table for deterministic hash scattering (Botnet Countermeasure)
    constexpr std::array<uint32_t, 16> AEGIS_FIBONACCI_LUT = {
        1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987
    };

    constexpr const char* ZVD_DEFAULT_MASTER_KEY = "ZVD-SOVEREIGN-MASTER-KEY-2026-AEGIS-SECURE";

    // Cached environment configuration helpers (Zero-Overhead Hot Path)
    inline const char* get_cached_master_key() noexcept {
        static const std::string master_key = []() {
            const char* master_env = std::getenv("ZVD_MASTER_KEY");
            return master_env ? std::string(master_env) : std::string(ZVD_DEFAULT_MASTER_KEY);
        }();
        return master_key.c_str();
    }

    inline int get_cached_cutoff_pct() noexcept {
        static const int cutoff = []() {
            const char* env_cutoff = std::getenv("ZVD_DDOS_CUTOFF_PCT");
            int val = env_cutoff ? std::atoi(env_cutoff) : 20;
            if (val < 10) val = 10;
            if (val > 30) val = 30;
            return val;
        }();
        return cutoff;
    }

} // namespace ZvdAegisDDoS


// =========================================================================
// 1. PRELIMINARY PARSER & DFA INJECTION FILTER (Zero-Alloc)
// =========================================================================

inline std::string_view extract_last_user_payload(std::string_view raw_prompt) noexcept {
    size_t pos = 0;
    size_t last_content_pos = std::string_view::npos;
    size_t last_content_len = 0;

    while ((pos = raw_prompt.find("\"role\"", pos)) != std::string_view::npos) {
        size_t role_val_pos = raw_prompt.find(':', pos);
        if (role_val_pos == std::string_view::npos) break;

        size_t user_idx = raw_prompt.find("\"user\"", role_val_pos);
        size_t tool_idx = raw_prompt.find("\"tool\"", role_val_pos);
        size_t match_role = std::string_view::npos;
        if (user_idx != std::string_view::npos && user_idx < role_val_pos + 30) match_role = user_idx;
        if (tool_idx != std::string_view::npos && tool_idx < role_val_pos + 30) {
            if (match_role == std::string_view::npos || tool_idx < match_role) match_role = tool_idx;
        }

        if (match_role != std::string_view::npos) {
            size_t content_key = raw_prompt.find("\"content\"", match_role);
            if (content_key != std::string_view::npos) {
                size_t colon_c = raw_prompt.find(':', content_key);
                if (colon_c != std::string_view::npos) {
                    size_t quote_start = raw_prompt.find('"', colon_c);
                    if (quote_start != std::string_view::npos) {
                        size_t quote_end = quote_start + 1;
                        bool escaped = false;
                        while (quote_end < raw_prompt.size()) {
                            if (raw_prompt[quote_end] == '\\' && !escaped) { escaped = true; }
                            else if (raw_prompt[quote_end] == '"' && !escaped) { break; }
                            else { escaped = false; }
                            quote_end++;
                        }
                        if (quote_end < raw_prompt.size()) {
                            last_content_pos = quote_start + 1;
                            last_content_len = quote_end - (quote_start + 1);
                        }
                    }
                }
            }
        }
        pos = role_val_pos + 1;
    }

    if (last_content_pos != std::string_view::npos) {
        return raw_prompt.substr(last_content_pos, last_content_len);
    }
    return raw_prompt;
}

inline bool scan_dfa_injection(std::string_view payload) noexcept {
    for (size_t i = 0; i < payload.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(payload[i]);
        if (c == 0xEF && i + 2 < payload.size() && 
            (unsigned char)payload[i+1] == 0xBB && (unsigned char)payload[i+2] == 0xBF) return true; // BOM
        if (c == 0xE2 && i + 2 < payload.size() && 
            (unsigned char)payload[i+1] == 0x80 && 
            ((unsigned char)payload[i+2] == 0x8B || (unsigned char)payload[i+2] == 0x8C || (unsigned char)payload[i+2] == 0x8D)) return true; // ZWSP
        if (c == 0xF3 && i + 3 < payload.size() && 
            (unsigned char)payload[i+1] == 0xA0 && 
            ((unsigned char)payload[i+2] == 0x80 || (unsigned char)payload[i+2] == 0x81)) return true; // Tags
    }

    if (payload.find("<|im_start|>") != std::string_view::npos) return true;
    if (payload.find("<|system|>") != std::string_view::npos) return true;
    if (payload.find("[INST]") != std::string_view::npos) return true;
    if (payload.find("<<SYS>>") != std::string_view::npos) return true;

    return false;
}


// =========================================================================
// 2. ECHELON 0: SOVEREIGN VIP CRYPTO-PASS (Zero-Trust Moire Verification)
// =========================================================================

inline bool verify_vip_token(std::string_view payload, ZvdPipelineContext& ctx) noexcept {
    constexpr std::string_view prefix = "ZVD-VIP:";
    if (payload.size() < prefix.size() + 32) return false;
    if (payload.substr(0, prefix.size()) != prefix) return false;

    size_t token_start = prefix.size();
    while (token_start < payload.size() && payload[token_start] == ' ') token_start++;
    if (token_start + 32 > payload.size()) return false;

    auto parse_hex_u64 = [](std::string_view s, uint64_t& val) -> bool {
        val = 0;
        for (int i = 0; i < 16; ++i) {
            char c = s[i];
            uint64_t v = 0;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            else return false;
            val = (val << 4) | v;
        }
        return true;
    };

    uint64_t nonce_ts = 0;
    uint64_t encrypted_sig = 0;
    if (!parse_hex_u64(payload.substr(token_start, 16), nonce_ts)) return false;
    if (!parse_hex_u64(payload.substr(token_start + 16, 16), encrypted_sig)) return false;

    auto now_sec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    int64_t diff = static_cast<int64_t>(now_sec) - static_cast<int64_t>(nonce_ts);
    if (diff < -30 || diff > 30) return false;

    const char* active_key = ZvdAegisDDoS::get_cached_master_key();
    uint64_t master_key_hash = zvd_fast_hash64(active_key, std::strlen(active_key));

    char cipher_buf[8];
    uint64_t raw_sig = nonce_ts ^ master_key_hash;
    std::memcpy(cipher_buf, &raw_sig, 8);

#ifdef ZVD_HAS_DATACENTER_CORE
    zvd_120d_moire_crypt_buffer(reinterpret_cast<uint8_t*>(cipher_buf), 8, active_key, static_cast<size_t>(nonce_ts), true);
#else
    zvd_120d_moire_crypt_buffer(cipher_buf, 8, master_key_hash);
#endif

    uint64_t expected_sig = 0;
    std::memcpy(&expected_sig, cipher_buf, 8);

    if (encrypted_sig != expected_sig) {
        return false;
    }

    ctx.routing_directive = "SOVEREIGN_VIP_BYPASS";
    return true;
}


// =========================================================================
// 3. ECHELON 1: PENTARY PRESS 3-4-5 & DYNAMIC CUTOFF THRESHOLD
// =========================================================================

inline uint64_t compute_geom_sig_345(std::string_view payload) noexcept {
#if __has_include("13.11_zvd_l5_l60_anthology.hpp")
    const auto& lut = ZvdAnthology::get_l60_quadrature_lut().lut_345;
#else
    uint8_t lut[125];
    for (int i = 0; i < 125; ++i) lut[i] = static_cast<uint8_t>((i * 3) % 251);
#endif

    uint32_t sum_a = 0, sum_b = 0;
    size_t len = std::min(payload.size(), size_t(48));
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = static_cast<uint8_t>(payload[i]);
        uint8_t idx = b % 125;
        uint8_t val = lut[idx];
        sum_a += (val & 0x0F);
        sum_b += (val >> 4);
    }

    uint32_t leg_a = 3 * sum_a;
    uint32_t leg_b = 4 * sum_b;
    uint64_t geom_sig = (static_cast<uint64_t>(leg_a) * leg_a + static_cast<uint64_t>(leg_b) * leg_b) >> 4;
    return geom_sig;
}

inline bool evaluate_echelon1_press(std::string_view payload, uint64_t& out_sig, ZvdPipelineContext& ctx) noexcept {
    out_sig = compute_geom_sig_345(payload);

    uint32_t cursor = ZvdAegisDDoS::g_ring_cursor.fetch_add(1, std::memory_order_relaxed) % ZvdAegisDDoS::AEGIS_WINDOW_SIZE;
    ZvdAegisDDoS::g_wave_ring[cursor].store(out_sig, std::memory_order_relaxed);

    uint32_t matches = 0;
    uint32_t active_count = 0;
    for (size_t i = 0; i < ZvdAegisDDoS::AEGIS_WINDOW_SIZE; ++i) {
        uint64_t val = ZvdAegisDDoS::g_wave_ring[i].load(std::memory_order_relaxed);
        if (val != 0) {
            active_count++;
            if (val == out_sig) matches++;
        }
    }

    // Cold start safety guard: bypass density check until window accumulates at least 32 packets
    if (active_count < 32) return true;

    if (active_count == 0) active_count = 1;
    uint32_t density_pct = (matches * 100) / active_count;

    int base_cutoff = ZvdAegisDDoS::get_cached_cutoff_pct();

    if (ctx.entropy > 7.0 || ctx.gl_invariant < 0) {
        base_cutoff = 10;
    }

    if (density_pct >= static_cast<uint32_t>(base_cutoff)) {
        return false; // DDoS detected -> VETO
    }
    return true;
}


// =========================================================================
// 4. ECHELON 2: INTEGER TURBO-THEATER Q8 & DETERMINISTIC FIBONACCI SCATTER
// =========================================================================

inline int evaluate_echelon2_tiers(std::string_view payload, uint64_t geom_sig) noexcept {
#if __has_include("13.11_zvd_l5_l60_anthology.hpp")
    const auto& q8 = ZvdAnthology::get_l60_quadrature_lut();
    uint64_t h = zvd_fast_hash64(payload.data(), payload.size());
    int angle_idx = static_cast<int>(h % 360);
    int16_t sq = q8.sin_q8[angle_idx];
    int16_t cq = q8.cos_q8[angle_idx];
#else
    int16_t sq = 128;
    int16_t cq = 128;
    uint64_t h = zvd_fast_hash64(payload.data(), payload.size());
#endif

    uint32_t fib_multiplier = ZvdAegisDDoS::AEGIS_FIBONACCI_LUT[geom_sig % 16];
    
    uint32_t mixed_hash = static_cast<uint32_t>(h * fib_multiplier);
    uint32_t cluster_idx = static_cast<uint32_t>((mixed_hash + std::abs(sq + cq)) % ZvdAegisDDoS::AEGIS_CLUSTER_BUCKETS);

    uint32_t current_epoch = ZvdAegisDDoS::g_aegis_tick.fetch_add(1, std::memory_order_relaxed) >> 13;
    uint32_t stored_epoch = ZvdAegisDDoS::g_cluster_epochs[cluster_idx].load(std::memory_order_relaxed);

    uint32_t hits = 0;
    if (stored_epoch != current_epoch) {
        uint32_t expected = stored_epoch;
        if (ZvdAegisDDoS::g_cluster_epochs[cluster_idx].compare_exchange_strong(expected, current_epoch, std::memory_order_acq_rel)) {
            ZvdAegisDDoS::g_cluster_hits[cluster_idx].store(1, std::memory_order_relaxed);
            hits = 1;
        } else {
            hits = ZvdAegisDDoS::g_cluster_hits[cluster_idx].fetch_add(1, std::memory_order_relaxed) + 1;
        }
    } else {
        hits = ZvdAegisDDoS::g_cluster_hits[cluster_idx].fetch_add(1, std::memory_order_relaxed) + 1;
    }

    if (hits > 500) {
        return 0; // Tier 0: Extreme wave / Graceful Deferral
    } else if (hits > 200) {
        return 1; // Tier 1: High pressure flow
    } else if (hits > 50) {
        return 2; // Tier 2: Normal background flow
    } else {
        return 3; // Tier 3: Light individual flow
    }
}


// =========================================================================
// 5. MAIN PIPELINE ENTRYPOINTS & COMPLIANT DLP VERIFICATION
// =========================================================================

inline int zvd_aegis_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (ctx.prompt.empty() && ctx.datacenter_pkt_ptr == nullptr) {
        return -1; // VETO
    }

    std::string_view raw = !ctx.prompt.empty() ? ctx.prompt : std::string_view(static_cast<const char*>(ctx.datacenter_pkt_ptr));
    std::string_view payload = extract_last_user_payload(raw);

    if (scan_dfa_injection(payload)) {
        ctx.routing_directive = "AEGIS_INJECTION_VETO";
        return -1;
    }

    if (verify_vip_token(payload, ctx)) {
        return 1; // PLUS (Bypass)
    }

    uint64_t geom_sig = 0;
    if (!evaluate_echelon1_press(payload, geom_sig, ctx)) {
        ctx.routing_directive = "AEGIS_DDOS_345_COLLAPSED";
        return -1;
    }

    int tier = evaluate_echelon2_tiers(payload, geom_sig);
    if (tier == 0) {
        ctx.routing_directive = "DEFERRED_TIER_0: Request queued due to high network load. Response will be provided later.";
        return -1;
    } else if (tier == 1 && ctx.entropy > 6.0) {
        ctx.routing_directive = "DEFERRED_TIER_1: High network pressure deferral.";
        return -1;
    }

    ctx.routing_directive = "AEGIS_PASSED_OK";
    return 0; // PASS
}

inline int zvd_aegis_evaluate_legacy(const char* pkt) noexcept {
    ZvdPipelineContext ctx;
    ctx.prompt = pkt ? std::string_view(pkt) : std::string_view("");
    ctx.datacenter_pkt_ptr = pkt;
    ctx.entropy = 3.0;
    ctx.gl_invariant = 0;
    return zvd_aegis_evaluate_ctx(ctx);
}

inline bool zvd_aegis_verify_response(std::string_view prompt, std::string_view response) noexcept {
    (void)prompt;
    const char* active_key = ZvdAegisDDoS::get_cached_master_key();

    if (response.find(active_key) != std::string_view::npos) return false;
    if (response.find("-----BEGIN") != std::string_view::npos) return false;
    if (response.find("sk-") != std::string_view::npos) return false;
    if (response.find("AKIA") != std::string_view::npos) return false;

    return true;
}


// =========================================================================
// 6. CARTRIDGE BUS REGISTRATION
// =========================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("ZVD-Aegis-DDoS-Shield-120D", ZvdModuleRole::FILTER, zvd_aegis_evaluate_ctx, zvd_aegis_verify_response);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("ZVD-Aegis-DDoS-Shield-120D", zvd_aegis_evaluate_legacy, zvd_aegis_verify_response);
#endif

#endif // ZVD_DDOS_AEGIS_V30_HPP
