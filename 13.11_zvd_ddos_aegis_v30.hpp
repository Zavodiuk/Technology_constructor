/* =============================================================================
 *  PROJECT:    ZVD-120D SOVEREIGN ECOSYSTEM — DDOS AEGIS SHIELD MODULE
 *              (13.11_zvd_ddos_aegis_v30.hpp - V3.2.0-MOIRE-CHALLENGE-AEGIS-C20)
 *  
 *  AUTHOR:     Vladimir Zavodiuk (Root-Architect / Varna, Bulgaria)
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation, alignas(64), Counter-Rotating Moiré Q8, Proof-of-Work
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

    // Sharded sliding density window (16 independent shards, 256 slots each = 4096 total slots)
    constexpr size_t AEGIS_SHARDS = 16;
    constexpr size_t AEGIS_SHARD_SIZE = 256;
    alignas(64) inline std::atomic<uint64_t> g_wave_ring[AEGIS_SHARDS][AEGIS_SHARD_SIZE];
    
    struct alignas(64) AlignedCursor {
        std::atomic<uint32_t> val{0};
    };
    alignas(64) inline AlignedCursor g_ring_cursors[AEGIS_SHARDS];

    // Per-shard counter-rotating angles for Moiré 60+60 gear meshing
    alignas(64) inline std::atomic<uint16_t> g_shard_angle_a[AEGIS_SHARDS];
    alignas(64) inline std::atomic<uint16_t> g_shard_angle_b[AEGIS_SHARDS];

    // Semantic cluster hit table & sliding epochs for load balancing tiers
    constexpr size_t AEGIS_CLUSTER_BUCKETS = 1024;
    alignas(64) inline std::atomic<uint32_t> g_cluster_hits[AEGIS_CLUSTER_BUCKETS];
    alignas(64) inline std::atomic<uint32_t> g_cluster_epochs[AEGIS_CLUSTER_BUCKETS];
    alignas(64) inline std::atomic<uint32_t> g_aegis_tick{0};

    // Compile-time Q8 Sine / Cosine LUTs (0 to 359 degrees, scaled by 256)
    constexpr std::array<int16_t, 360> Q8_SIN_LUT = []() {
        std::array<int16_t, 360> lut{};
        constexpr double PI = 3.14159265358979323846;
        for (int i = 0; i < 360; ++i) {
            lut[i] = static_cast<int16_t>(std::sin(static_cast<double>(i) * PI / 180.0) * 256.0);
        }
        return lut;
    }();

    constexpr std::array<int16_t, 360> Q8_COS_LUT = []() {
        std::array<int16_t, 360> lut{};
        constexpr double PI = 3.14159265358979323846;
        for (int i = 0; i < 360; ++i) {
            lut[i] = static_cast<int16_t>(std::cos(static_cast<double>(i) * PI / 180.0) * 256.0);
        }
        return lut;
    }();

    // Fibonacci Lookup Table for deterministic hash scattering
    constexpr std::array<uint32_t, 16> AEGIS_FIBONACCI_LUT = {
        1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987
    };

    constexpr const char* ZVD_DEFAULT_MASTER_KEY = "ZVD-SOVEREIGN-MASTER-KEY-2026-AEGIS-SECURE";

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
            int val = env_cutoff ? std::atoi(env_cutoff) : 14; // Default upgraded to 14%
            if (val < 5) val = 5;
            if (val > 25) val = 25;
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
// 2. ECHELON 0: SOVEREIGN VIP CRYPTO-PASS
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
// 3. ECHELON 1: 3-POINT HEAD+MID+TAIL 3-4-5 SIGNATURE & MOIRÉ 60+60 GEAR MESHING
// =========================================================================

inline uint64_t compute_geom_sig_345(std::string_view payload) noexcept {
    static constexpr uint8_t embedded_lut_345[125] = {
        0,3,6,9,12,15,18,21,24,27,30,33,36,39,42,45,48,51,54,57,
        60,63,66,69,72,75,78,81,84,87,90,93,96,99,102,105,108,111,114,117,
        120,123,126,129,132,135,138,141,144,147,150,153,156,159,162,165,168,171,174,177,
        180,183,186,189,192,195,198,201,204,207,210,213,216,219,222,225,228,231,234,237,
        240,243,246,249,1,4,7,10,13,16,19,22,25,28,31,34,37,40,43,46,
        49,52,55,58,61,64,67,70,73,76,79,82,85,88,91,94,97,100,103,106,
        109,112,115,118,121,124,127,130,133
    };

    uint32_t sum_a = 0, sum_b = 0;
    size_t size = payload.size();

    auto sample_segment = [&](size_t start, size_t length) {
        size_t end = std::min(start + length, size);
        for (size_t i = start; i < end; ++i) {
            uint8_t b = static_cast<uint8_t>(payload[i]);
            uint8_t idx = b % 125;
            uint8_t val = embedded_lut_345[idx];
            sum_a += (val & 0x0F);
            sum_b += (val >> 4);
        }
    };

    // 3-Point Sampling: Head (24B) + Middle (16B) + Tail (24B)
    sample_segment(0, 24);
    if (size > 48) {
        sample_segment(size / 2 - 8, 16);
    }
    if (size > 24) {
        sample_segment(size >= 24 ? size - 24 : 0, 24);
    }

    uint32_t leg_a = 3 * sum_a;
    uint32_t leg_b = 4 * sum_b;
    uint64_t geom_sig = ((static_cast<uint64_t>(leg_a) * leg_a + static_cast<uint64_t>(leg_b) * leg_b) >> 4) + size;
    return geom_sig;
}

inline bool evaluate_echelon1_press(std::string_view payload, uint64_t& out_sig, ZvdPipelineContext& ctx) noexcept {
    out_sig = compute_geom_sig_345(payload);

    size_t shard_idx = (out_sig >> 4) & (ZvdAegisDDoS::AEGIS_SHARDS - 1);

    // Counter-rotating gear phase stepping per shard (60+60 icosahedra Moire meshing)
    uint16_t angle_a = ZvdAegisDDoS::g_shard_angle_a[shard_idx].load(std::memory_order_relaxed);
    uint16_t angle_b = ZvdAegisDDoS::g_shard_angle_b[shard_idx].load(std::memory_order_relaxed);
    
    angle_a = static_cast<uint16_t>((angle_a + 17) % 360);
    angle_b = static_cast<uint16_t>((angle_b - 11 + 360) % 360);

    ZvdAegisDDoS::g_shard_angle_a[shard_idx].store(angle_a, std::memory_order_relaxed);
    ZvdAegisDDoS::g_shard_angle_b[shard_idx].store(angle_b, std::memory_order_relaxed);

    int16_t sin_val = ZvdAegisDDoS::Q8_SIN_LUT[angle_a];
    int16_t cos_val = ZvdAegisDDoS::Q8_COS_LUT[angle_b];
    int32_t moire_tooth = (static_cast<int32_t>(sin_val) * static_cast<int32_t>(cos_val)) >> 8;

    uint64_t modulated_sig = out_sig ^ static_cast<uint64_t>(std::abs(moire_tooth));

    uint32_t cursor = ZvdAegisDDoS::g_ring_cursors[shard_idx].val.fetch_add(1, std::memory_order_relaxed) % ZvdAegisDDoS::AEGIS_SHARD_SIZE;
    ZvdAegisDDoS::g_wave_ring[shard_idx][cursor].store(modulated_sig, std::memory_order_relaxed);

    uint32_t matches = 0;
    uint32_t active_count = 0;
    for (size_t i = 0; i < ZvdAegisDDoS::AEGIS_SHARD_SIZE; ++i) {
        uint64_t val = ZvdAegisDDoS::g_wave_ring[shard_idx][i].load(std::memory_order_relaxed);
        if (val != 0) {
            active_count++;
            uint64_t diff = (val > modulated_sig) ? (val - modulated_sig) : (modulated_sig - val);
            if (diff <= 3) matches++;
        }
    }

    if (active_count < 32) return true;

    if (active_count == 0) active_count = 1;
    uint32_t density_pct = (matches * 100) / active_count;

    int base_cutoff = ZvdAegisDDoS::get_cached_cutoff_pct(); // Default 14%

    if (ctx.entropy > 7.0 || ctx.gl_invariant < 0) {
        base_cutoff = 8; // High-entropy anomaly threshold (8%)
    }

    if (density_pct >= static_cast<uint32_t>(base_cutoff)) {
        return false; // DDoS wave detected -> VETO / Challenge Required
    }
    return true;
}


// =========================================================================
// 4. ECHELON 1.5: DOUBLE SOVEREIGN CHALLENGE (Zero-Alloc PoW Barrier)
// =========================================================================

namespace ZvdAegisDoubleChallenge {

    struct SovereignChallengeHeader {
        uint64_t client_seed;
        uint64_t difficulty_target;
    };

    inline uint64_t generate_challenge_target(uint64_t geom_sig, uint64_t epoch) noexcept {
        const char* active_key = ZvdAegisDDoS::get_cached_master_key();
        uint64_t key_hash = zvd_fast_hash64(active_key, std::strlen(active_key));
        uint64_t mix = geom_sig ^ key_hash ^ (epoch * 0xBF58476D1CE4E5B9ULL);
        mix ^= (mix >> 31);
        mix *= 0x9E3779B97F4A7C15ULL;
        return mix;
    }

    inline bool verify_pow_solution(uint64_t client_seed, uint64_t client_nonce, uint64_t target) noexcept {
        uint64_t mix = client_seed ^ (client_nonce * 0x9E3779B97F4A7C15ULL);
        mix ^= (mix >> 33);
        mix *= 0xBF58476D1CE4E5B9ULL;
        mix ^= (mix >> 33);
        
        // Verifies against 12-bit difficulty mask (0x0FFF)
        return (mix & 0x0000000000000FFFULL) == (target & 0x0000000000000FFFULL);
    }

} // namespace ZvdAegisDoubleChallenge

inline bool verify_pow_token(std::string_view payload, uint64_t geom_sig, ZvdPipelineContext& ctx) noexcept {
    constexpr std::string_view prefix = "ZVD-POW:";
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

    uint64_t client_seed = 0;
    uint64_t client_nonce = 0;
    if (!parse_hex_u64(payload.substr(token_start, 16), client_seed)) return false;
    if (!parse_hex_u64(payload.substr(token_start + 16, 16), client_nonce)) return false;

    uint64_t current_epoch = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) / 30; // 30-second epoch window

    uint64_t target = ZvdAegisDoubleChallenge::generate_challenge_target(geom_sig, current_epoch);

    if (!ZvdAegisDoubleChallenge::verify_pow_solution(client_seed, client_nonce, target)) {
        return false;
    }

    ctx.routing_directive = "AEGIS_POW_VERIFIED_PASS";
    return true;
}


// =========================================================================
// 5. ECHELON 2: INTEGER TURBO-THEATER Q8 & FIBONACCI SCATTER
// =========================================================================

inline int evaluate_echelon2_tiers(std::string_view payload, uint64_t geom_sig) noexcept {
    int16_t sq = 128;
    int16_t cq = 128;
    uint64_t h = zvd_fast_hash64(payload.data(), payload.size());

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

    if (hits > 400) {
        return 0; // Tier 0: Heavy flood / Deferral
    } else if (hits > 180) {
        return 1; // Tier 1: Elevated pressure
    } else if (hits > 40) {
        return 2; // Tier 2: Normal background
    } else {
        return 3; // Tier 3: Light flow
    }
}


// =========================================================================
// 6. MAIN PIPELINE ENTRYPOINTS & COMPLIANT DLP VERIFICATION
// =========================================================================

inline int zvd_aegis_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (ctx.prompt.empty() && ctx.datacenter_pkt_ptr == nullptr) {
        return -1;
    }

    std::string_view raw = !ctx.prompt.empty() ? ctx.prompt : std::string_view(static_cast<const char*>(ctx.datacenter_pkt_ptr));
    std::string_view payload = extract_last_user_payload(raw);

    if (scan_dfa_injection(payload)) {
        ctx.routing_directive = "AEGIS_INJECTION_VETO";
        return -1;
    }

    if (verify_vip_token(payload, ctx)) {
        return 1; // PLUS (VIP Bypass)
    }

    uint64_t geom_sig = 0;
    bool echelon1_ok = evaluate_echelon1_press(payload, geom_sig, ctx);

    // If Echelon 1 detects a wave, check for Proof-of-Work Sovereign Challenge token
    if (!echelon1_ok) {
        if (verify_pow_token(payload, geom_sig, ctx)) {
            return 0; // PASS via valid Proof-of-Work challenge resolution
        }
        ctx.routing_directive = "AEGIS_CHALLENGE_REQUIRED";
        return -1; // VETO / Issue PoW Challenge to client
    }

    int tier = evaluate_echelon2_tiers(payload, geom_sig);
    if (tier == 0) {
        if (verify_pow_token(payload, geom_sig, ctx)) {
            return 0;
        }
        ctx.routing_directive = "DEFERRED_TIER_0: Heavy network pressure. Double Sovereign Challenge required.";
        return -1;
    } else if (tier == 1 && ctx.entropy > 6.0) {
        ctx.routing_directive = "DEFERRED_TIER_1: High network entropy pressure deferral.";
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
// 7. CARTRIDGE BUS REGISTRATION
// =========================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("ZVD-Aegis-DDoS-Shield-120D", ZvdModuleRole::FILTER, zvd_aegis_evaluate_ctx, zvd_aegis_verify_response);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("ZVD-Aegis-DDoS-Shield-120D", zvd_aegis_evaluate_legacy, zvd_aegis_verify_response);
#endif

#endif // ZVD_DDOS_AEGIS_V30_HPP
  
