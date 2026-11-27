/* =============================================================================
 *  PROJECT:    ZVD-120D SOVEREIGN ECOSYSTEM — DDOS AEGIS SHIELD MODULE
 *              (13.11_zvd_ddos_aegis_v30.hpp - V3.3.0-SUPREME-C20)
 *
 *  AUTHOR:     Vladimir Zavodiuk (Root-Architect / Varna, Bulgaria)
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution,
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation, alignas(64), Dual-Phase Moiré Q8, Proof-of-Work
 * =============================================================================
 */

#ifndef ZVD_DDOS_AEGIS_V30_HPP
#define ZVD_DDOS_AEGIS_V30_HPP

#include <cmath>
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
#include <cerrno>

// Fallback compatibility context if included outside the core orchestrator
#ifndef ZVD_HAS_DATACENTER_CORE
struct ZvdPipelineContext {
    std::string_view prompt;
    const void* datacenter_pkt_ptr;
    std::string routing_directive;
    double entropy;
    int8_t gl_invariant;
};

inline uint64_t zvd_fast_hash64(const uint8_t* data, size_t len) noexcept {
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
    static_assert(AEGIS_SHARD_SIZE > 0 && (AEGIS_SHARD_SIZE & (AEGIS_SHARD_SIZE - 1)) == 0,
                  "AEGIS_SHARD_SIZE must be a non-zero power of two");

    constexpr double AEGIS_DUAL_ASYMMETRY_LIMIT = 2.2;
    constexpr double AEGIS_RISK_INDEX_LIMIT = -1.8;
    constexpr int AEGIS_ELEVATED_THREAT_CUTOFF_PCT = 6;

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

    // C++20 compliant runtime-initialized Q8 Sine / Cosine LUTs (0 to 359 degrees, scaled by 256)
    inline const std::array<int16_t, 360> Q8_SIN_LUT = []() {
        std::array<int16_t, 360> lut{};
        constexpr double PI = 3.14159265358979323846;
        for (int i = 0; i < 360; ++i) {
            lut[i] = static_cast<int16_t>(std::sin(static_cast<double>(i) * PI / 180.0) * 256.0);
        }
        return lut;
    }();

    inline const std::array<int16_t, 360> Q8_COS_LUT = []() {
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

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD-SOVEREIGN-ENTERPRISE-2026"
#endif

    inline const char* get_cached_master_key() noexcept {
        static const std::string master_key = []() {
            const char* master_env = std::getenv("ZVD_MASTER_KEY");
            return (master_env && master_env[0] != '\0')
                ? std::string(master_env)
                : std::string(ZVD_DEFAULT_MASTER_KEY);
        }();
        return master_key.c_str();
    }

    inline uint64_t get_cached_master_key_hash() noexcept {
        static const uint64_t key_hash = []() {
            const char* k = get_cached_master_key();
            return zvd_fast_hash64(reinterpret_cast<const uint8_t*>(k), std::strlen(k));
        }();
        return key_hash;
    }

    inline int get_cached_cutoff_pct() noexcept {
        static const int cutoff = []() {
            const char* env_cutoff = std::getenv("ZVD_DDOS_CUTOFF_PCT");
            if (!env_cutoff || env_cutoff[0] == '\0') return 14;
            errno = 0;
            char* endptr = nullptr;
            long parsed = std::strtol(env_cutoff, &endptr, 10);
            int val = (errno == 0 && endptr != env_cutoff && *endptr == '\0')
                ? static_cast<int>(parsed)
                : 14;
            if (val < 5) val = 5;
            if (val > 25) val = 25;
            return val;
        }();
        return cutoff;
    }

    inline uint64_t current_epoch_30s() noexcept {
        auto now_sec = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return now_sec / 30ULL;
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
            static_cast<unsigned char>(payload[i + 1]) == 0xBB &&
            static_cast<unsigned char>(payload[i + 2]) == 0xBF) return true;
        if (c == 0xE2 && i + 2 < payload.size() &&
            static_cast<unsigned char>(payload[i + 1]) == 0x80 &&
            (static_cast<unsigned char>(payload[i + 2]) == 0x8B ||
             static_cast<unsigned char>(payload[i + 2]) == 0x8C ||
             static_cast<unsigned char>(payload[i + 2]) == 0x8D)) return true;
        if (c == 0xF3 && i + 3 < payload.size() &&
            static_cast<unsigned char>(payload[i + 1]) == 0xA0 &&
            (static_cast<unsigned char>(payload[i + 2]) == 0x80 ||
             static_cast<unsigned char>(payload[i + 2]) == 0x81)) return true;
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
            if (c >= '0' && c <= '9') v = static_cast<uint64_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v = static_cast<uint64_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = static_cast<uint64_t>(c - 'A' + 10);
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
    uint64_t master_key_hash = ZvdAegisDDoS::get_cached_master_key_hash();

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

    ctx.routing_directive = "AEGIS_VIP_PASS";
    return true;
}


// =========================================================================
// 3. ECHELON 1: 3-POINT HEAD+MID+TAIL SIGNATURE & DUAL-PHASE MOIRÉ Q8
// =========================================================================

inline uint64_t compute_geom_sig_345(std::string_view payload) noexcept {
    static constexpr uint8_t embedded_lut_345[125] = {
        0,3,6,9,12,15,18,21,24,27,30,33,36,39,42,45,48,51,54,57,
        60,63,66,69,72,75,78,81,84,87,90,93,96,99,102,105,108,111,114,117,
        120,123,126,129,132,135,138,141,144,147,150,153,156,159,162,165,168,171,174,177,
        180,183,186,189,192,195,198,201,204,207,210,213,216,219,222,225,228,231,234,237,
        240,243,246,249,1,4,7,10,13,16,19,22,25,28,31,34,37,40,43,46,
        49,52,55,58,61,64,67,70,73,76,79,82,85,88,91,94,97,100,103,106,
        109,112,115,118,121
    };

    uint32_t sum_a = 0, sum_b = 0;
    size_t size = payload.size();

    auto sample_segment = [&](size_t start, size_t length) {
        size_t end = std::min(start + length, size);
        for (size_t i = start; i < end; ++i) {
            uint8_t b = static_cast<uint8_t>(payload[i]);
            uint8_t idx = static_cast<uint8_t>(b % 125u);
            uint8_t val = embedded_lut_345[idx];
            sum_a += static_cast<uint32_t>((val & 0x0Fu) * ((i & 7u) + 1u));
            sum_b += static_cast<uint32_t>((val >> 4) * ((i & 7u) + 1u));
        }
    };

    sample_segment(0, 24);
    if (size > 48) {
        sample_segment(size / 2 - 8, 16);
    }
    if (size > 24) {
        sample_segment(size >= 24 ? size - 24 : 0, 24);
    }

    uint32_t leg_a = 3u * sum_a;
    uint32_t leg_b = 4u * sum_b;
    uint64_t geom_sig = (static_cast<uint64_t>(leg_a) * leg_a + static_cast<uint64_t>(leg_b) * leg_b) + size;
    return geom_sig;
}

inline bool evaluate_echelon1_press(std::string_view payload, uint64_t& out_sig, ZvdPipelineContext& ctx) noexcept {
    out_sig = compute_geom_sig_345(payload);

    size_t shard_idx = static_cast<size_t>((out_sig >> 4) & (ZvdAegisDDoS::AEGIS_SHARDS - 1u));

    uint16_t angle_a = ZvdAegisDDoS::g_shard_angle_a[shard_idx].load(std::memory_order_relaxed);
    uint16_t angle_b = ZvdAegisDDoS::g_shard_angle_b[shard_idx].load(std::memory_order_relaxed);

    angle_a = static_cast<uint16_t>((angle_a + 17u) % 360u);
    angle_b = static_cast<uint16_t>((angle_b + 360u - 11u) % 360u);

    ZvdAegisDDoS::g_shard_angle_a[shard_idx].store(angle_a, std::memory_order_relaxed);
    ZvdAegisDDoS::g_shard_angle_b[shard_idx].store(angle_b, std::memory_order_relaxed);

    int32_t sin_a = static_cast<int32_t>(ZvdAegisDDoS::Q8_SIN_LUT[angle_a]);
    int32_t cos_a = static_cast<int32_t>(ZvdAegisDDoS::Q8_COS_LUT[angle_a]);
    int32_t sin_b = static_cast<int32_t>(ZvdAegisDDoS::Q8_SIN_LUT[angle_b]);
    int32_t cos_b = static_cast<int32_t>(ZvdAegisDDoS::Q8_COS_LUT[angle_b]);

    int32_t beat1 = std::abs(sin_a + cos_b);
    int32_t beat2 = std::abs(cos_a - sin_b);
    int32_t dual_beat = (beat1 + beat2) >> 1;

    uint32_t cursor = ZvdAegisDDoS::g_ring_cursors[shard_idx].val.fetch_add(1u, std::memory_order_relaxed) &
                      static_cast<uint32_t>(ZvdAegisDDoS::AEGIS_SHARD_SIZE - 1u);
    ZvdAegisDDoS::g_wave_ring[shard_idx][cursor].store(out_sig, std::memory_order_relaxed);

    uint32_t matches = 0;
    uint32_t active_count = 0;
    for (size_t i = 0; i < ZvdAegisDDoS::AEGIS_SHARD_SIZE; ++i) {
        uint64_t val = ZvdAegisDDoS::g_wave_ring[shard_idx][i].load(std::memory_order_relaxed);
        if (val != 0) {
            active_count++;
            uint64_t diff = (val > out_sig) ? (val - out_sig) : (out_sig - val);
            if (diff <= 3ULL) {
                matches += (dual_beat > 180) ? 2u : 1u;
            }
        }
    }

    if (active_count < 32u) return true;

    if (active_count == 0u) active_count = 1u;
    uint32_t density_pct = (matches * 100u) / active_count;

    int base_cutoff = ZvdAegisDDoS::get_cached_cutoff_pct();
    double entropy_limit = (payload.size() < 256u) ? 6.6 : 7.2;

    if (ctx.entropy > entropy_limit || ctx.gl_invariant < 0) {
        base_cutoff = std::min(base_cutoff, 8);
    }

#ifdef ZVD_HAS_DATACENTER_CORE
    if (ctx.datacenter_pkt_ptr != nullptr) {
        const auto* pkt = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr);
        if (pkt->dual_asymmetry > ZvdAegisDDoS::AEGIS_DUAL_ASYMMETRY_LIMIT ||
            pkt->risk_index < ZvdAegisDDoS::AEGIS_RISK_INDEX_LIMIT) {
            base_cutoff = std::min(base_cutoff, ZvdAegisDDoS::AEGIS_ELEVATED_THREAT_CUTOFF_PCT);
        }
    }
#endif

    if (density_pct >= static_cast<uint32_t>(base_cutoff)) {
        return false;
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
        uint64_t key_hash = ZvdAegisDDoS::get_cached_master_key_hash();
        uint64_t mix = geom_sig ^ key_hash ^ (epoch * 0xBF58476D1CE4E5B9ULL);
        mix ^= (mix >> 31);
        mix *= 0x9E3779B97F4A7C15ULL;
        return mix;
    }

    inline bool verify_pow_solution(uint64_t client_seed, uint64_t client_nonce, uint64_t target, uint32_t expected_target) noexcept {
        if (static_cast<uint32_t>(client_seed) != expected_target) {
            return false;
        }

        uint64_t mix = client_seed ^ (client_nonce * 0x9E3779B97F4A7C15ULL);
        mix ^= (mix >> 33);
        mix *= 0xBF58476D1CE4E5B9ULL;
        mix ^= (mix >> 33);

        return (mix & 0x0000000000000FFFULL) == (target & 0x0000000000000FFFULL);
    }

} // namespace ZvdAegisDoubleChallenge

inline bool verify_pow_token(std::string_view payload, uint64_t geom_sig, uint32_t expected_target, ZvdPipelineContext& ctx) noexcept {
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
            if (c >= '0' && c <= '9') v = static_cast<uint64_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v = static_cast<uint64_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v = static_cast<uint64_t>(c - 'A' + 10);
            else return false;
            val = (val << 4) | v;
        }
        return true;
    };

    uint64_t client_seed = 0;
    uint64_t client_nonce = 0;
    if (!parse_hex_u64(payload.substr(token_start, 16), client_seed)) return false;
    if (!parse_hex_u64(payload.substr(token_start + 16, 16), client_nonce)) return false;

    uint64_t current_epoch = ZvdAegisDDoS::current_epoch_30s();
    uint64_t target = ZvdAegisDoubleChallenge::generate_challenge_target(geom_sig, current_epoch);

    if (!ZvdAegisDoubleChallenge::verify_pow_solution(client_seed, client_nonce, target, expected_target)) {
        return false;
    }

    ctx.routing_directive = "AEGIS_POW_PASS";
    return true;
}


// =========================================================================
// 5. ECHELON 2: STRUCTURAL SKELETON CLUSTERING (Polymorphic Botnet Defense)
// =========================================================================

inline int evaluate_echelon2_tiers(std::string_view payload, uint64_t geom_sig) noexcept {
    // 1. O(1) Lightweight Structural Skeleton Hash over the first 64 bytes
    size_t scan_len = std::min(payload.size(), size_t(64));
    uint32_t delim_signature = 0;
    for (size_t i = 0; i < scan_len; ++i) {
        char c = payload[i];
        if (c == '{' || c == '}') delim_signature += (c == '{' ? 1u : 2u);
        else if (c == ':') delim_signature += 4u;
        else if (c == '"') delim_signature += 8u;
        else if (c == '/') delim_signature += 16u;
        else if (c == '=') delim_signature += 32u;
        else if (c == '&') delim_signature += 64u;
    }

    // Quantized length metric
    uint32_t quantized_len = static_cast<uint32_t>(payload.size() >> 4);
    uint32_t skeleton_hash = delim_signature ^ (quantized_len * 37u);

    // 2. Combine structural skeleton hash with Fibonacci geometry scatter
    uint32_t fib_multiplier = ZvdAegisDDoS::AEGIS_FIBONACCI_LUT[static_cast<size_t>(geom_sig % 16ULL)];
    uint32_t cluster_idx = (skeleton_hash * fib_multiplier) % static_cast<uint32_t>(ZvdAegisDDoS::AEGIS_CLUSTER_BUCKETS);

    uint32_t current_epoch = ZvdAegisDDoS::g_aegis_tick.fetch_add(1u, std::memory_order_relaxed) >> 13;
    uint32_t stored_epoch = ZvdAegisDDoS::g_cluster_epochs[cluster_idx].load(std::memory_order_relaxed);

    uint32_t hits = 0;
    if (stored_epoch != current_epoch) {
        uint32_t expected = stored_epoch;
        if (ZvdAegisDDoS::g_cluster_epochs[cluster_idx].compare_exchange_strong(expected, current_epoch, std::memory_order_acq_rel)) {
            ZvdAegisDDoS::g_cluster_hits[cluster_idx].store(1u, std::memory_order_relaxed);
            hits = 1u;
        } else {
            hits = ZvdAegisDDoS::g_cluster_hits[cluster_idx].fetch_add(1u, std::memory_order_relaxed) + 1u;
        }
    } else {
        hits = ZvdAegisDDoS::g_cluster_hits[cluster_idx].fetch_add(1u, std::memory_order_relaxed) + 1u;
    }

    // Polymorphic botnet waves sharing template structure collapse here within 400 packets
    if (hits > 400u) {
        return 0; // DEFERRED_TIER_0 / Challenge required
    } else if (hits > 180u) {
        return 1;
    } else if (hits > 40u) {
        return 2;
    } else {
        return 3;
    }
}


// =========================================================================
// 6. MAIN PIPELINE ENTRYPOINTS & COMPLIANT DLP VERIFICATION
// =========================================================================

inline int zvd_aegis_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (ctx.prompt.empty()) {
        ctx.routing_directive = "AEGIS_EMPTY_VETO";
        return -1;
    }

    std::string_view raw = ctx.prompt;

    if (scan_dfa_injection(raw)) {
        ctx.routing_directive = "AEGIS_DFA_VETO";
        return -1;
    }

    std::string_view payload = extract_last_user_payload(raw);

    if (verify_vip_token(payload, ctx)) {
        return 1;
    }

    std::string_view clean_payload = payload;
    if (clean_payload.starts_with("ZVD-POW:")) {
        size_t space_pos = clean_payload.find(' ');
        if (space_pos != std::string_view::npos) {
            clean_payload = clean_payload.substr(space_pos + 1);
        }
    }

    uint64_t geom_sig = 0;
    bool echelon1_ok = evaluate_echelon1_press(clean_payload, geom_sig, ctx);

    if (!echelon1_ok) {
        uint64_t current_epoch = ZvdAegisDDoS::current_epoch_30s();
        uint32_t expected_target = static_cast<uint32_t>(
            ZvdAegisDoubleChallenge::generate_challenge_target(geom_sig, current_epoch));
        if (verify_pow_token(payload, geom_sig, expected_target, ctx)) {
            return 0;
        }
        ctx.routing_directive = "AEGIS_POW_CHAL";
        return -1;
    }

    int tier = evaluate_echelon2_tiers(clean_payload, geom_sig);
    if (tier == 0) {
        uint64_t current_epoch = ZvdAegisDDoS::current_epoch_30s();
        uint32_t expected_target = static_cast<uint32_t>(
            ZvdAegisDoubleChallenge::generate_challenge_target(geom_sig, current_epoch));
        if (verify_pow_token(payload, geom_sig, expected_target, ctx)) {
            return 0;
        }
        ctx.routing_directive = "AEGIS_POW_CHAL";
        return -1;
    } else if (tier == 1 && ctx.entropy > 6.0) {
        ctx.routing_directive = "AEGIS_FLOOD_DROP";
        return -1;
    }

    ctx.routing_directive = "AEGIS_OK";
    return 0;
}

inline int zvd_aegis_evaluate_legacy(const char* pkt) noexcept {
    ZvdPipelineContext ctx;
    ctx.prompt = pkt ? std::string_view(pkt) : std::string_view("");
    ctx.datacenter_pkt_ptr = nullptr;
    ctx.entropy = 3.0;
    ctx.gl_invariant = 0;
    return zvd_aegis_evaluate_ctx(ctx);
}

namespace {

inline bool zvd_dlp_is_token_boundary(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
           c == '"' || c == '\'' || c == '=' || c == ':' ||
           c == ',' || c == ';' || c == '(' || c == '[' || c == '{';
}

inline bool zvd_dlp_is_sk_char(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

inline bool zvd_dlp_is_akia_char(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

inline bool zvd_dlp_has_secret_key(std::string_view response) noexcept {
    size_t pos = 0;
    while ((pos = response.find("sk-", pos)) != std::string_view::npos) {
        bool valid_prefix_boundary = (pos == 0) || zvd_dlp_is_token_boundary(response[pos - 1]);
        if (valid_prefix_boundary) {
            size_t i = pos + 3;
            while (i < response.size() && zvd_dlp_is_sk_char(response[i])) {
                ++i;
            }
            if ((i - (pos + 3)) >= 20u) {
                return true;
            }
        }
        pos += 3;
    }
    return false;
}

inline bool zvd_dlp_has_aws_access_key(std::string_view response) noexcept {
    size_t pos = 0;
    while ((pos = response.find("AKIA", pos)) != std::string_view::npos) {
        bool valid_prefix_boundary = (pos == 0) || zvd_dlp_is_token_boundary(response[pos - 1]);
        if (valid_prefix_boundary) {
            size_t tail_start = pos + 4;
            if (tail_start + 16u <= response.size()) {
                bool all_16_valid = true;
                for (size_t i = 0; i < 16u; ++i) {
                    if (!zvd_dlp_is_akia_char(response[tail_start + i])) {
                        all_16_valid = false;
                        break;
                    }
                }
                bool clean_end = (tail_start + 16u == response.size()) ||
                                 !zvd_dlp_is_akia_char(response[tail_start + 16u]);
                if (all_16_valid && clean_end) {
                    return true;
                }
            }
        }
        pos += 4;
    }
    return false;
}

} // namespace

inline bool zvd_aegis_verify_response(std::string_view prompt, std::string_view response) noexcept {
    (void)prompt;
    const char* active_key = ZvdAegisDDoS::get_cached_master_key();

    if (response.find(active_key) != std::string_view::npos) return false;
    if (response.find("-----BEGIN") != std::string_view::npos) return false;
    if (zvd_dlp_has_secret_key(response)) return false;
    if (zvd_dlp_has_aws_access_key(response)) return false;

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
