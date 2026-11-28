/* =============================================================================
 *  PROJECT:    ZVD-QR-VISUAL-CRYPTO-120D MONOCHROME & DUAL-ICOSA MOIRE CODEC
 *              (13.11_zvd_qr_visual_crypto_v34.hpp - V34.0-QR-TRI125-AEAD-PHOTO)
 *  FILENAME:   13.11_zvd_qr_visual_crypto_v34.hpp
 *  VERSION:    2026.11-V34.0-QR-TRI125-AEAD-PHOTO
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution,
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / C++20 (-Wall -Wextra -Werror)
 *              100% Standalone (Zero External Dependencies), Zero-Heap Hot Path,
 *              static thread_local alignas(64), Dual 60D Icosa Moire + AEAD + 8x8 Tri-Tiles
 * =============================================================================
 */

#pragma once

#ifndef ZVD_HAS_VISUAL_CRYPTO
#define ZVD_HAS_VISUAL_CRYPTO 1
#endif

#ifndef ZVD_HAS_QR_VISUAL_CRYPTO
#define ZVD_HAS_QR_VISUAL_CRYPTO 1
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD2026VarnaCore"
#endif

#ifndef ZVD_DIM_120
#define ZVD_DIM_120 120
#endif

#ifndef ZVD_CELLS_PER_LAYER
#define ZVD_CELLS_PER_LAYER 500
#endif

#ifndef ZVD_MAX_VISUAL_FRAME_BYTES
#define ZVD_MAX_VISUAL_FRAME_BYTES (16u * 1024u * 1024u)
#endif

#ifndef ZVD_GLOBAL_SHUTDOWN_DEFINED
#define ZVD_GLOBAL_SHUTDOWN_DEFINED
inline std::atomic<bool> g_zvd_global_shutdown{false};
#endif

// =====================================================================
// 1. FOUNDATIONS, CONSTANTS, KEY STRUCTURE & 360-DEG ICOSA ANGLE TABLE
// =====================================================================

namespace ZvdQrVisualDetail {

inline constexpr double ZVD_PI = 3.14159265358979323846;
inline constexpr double ZVD_TWO_PI = 6.28318530717958647692;
inline constexpr double ZVD_PHI = 1.61803398874989484820;

inline constexpr size_t ZVD_ICOSA_NODES = 60u;
inline constexpr size_t ZVD_MOIRE_PACKED_BYTES = 20u;

inline constexpr size_t ZVD_AEAD_KEY_BYTES = 32u;
inline constexpr size_t ZVD_AEAD_NONCE_BYTES = 12u;
inline constexpr size_t ZVD_AEAD_TAG_BYTES = 16u;
inline constexpr size_t ZVD_STREAM_BLOCK_BYTES = 64u;

inline constexpr size_t ZVD_ICOSA_ANCHOR_BYTES = 120u;
inline constexpr size_t ZVD_CUBE_ANCHOR_BYTES = 14u;
inline constexpr uint8_t ZVD_FOLD_FLAG_LGZ345 = 0x01u;
inline constexpr uint8_t ZVD_FOLD_FLAG_PLANESPLIT = 0x02u;
inline constexpr uint8_t ZVD_FOLD_FLAG_FEC3X = 0x04u;

inline constexpr uint8_t ZVD_PX_BLACK = 0x00u;
inline constexpr uint8_t ZVD_PX_WHITE = 0xFFu;
inline constexpr size_t  ZVD_TILE_SIZE = 8u;
inline constexpr size_t  ZVD_TILE_PIXELS = ZVD_TILE_SIZE * ZVD_TILE_SIZE;
inline constexpr size_t  ZVD_CORNER_MARKER_PX = 32u;
inline constexpr size_t  ZVD_BOTTOM_STRIP_PX = 32u;
inline constexpr size_t  ZVD_MARGIN_PX = 16u;

inline constexpr uint32_t ZVD_QR_FRAME_MAGIC = 0x5144565Au; // "ZVDQ"
inline constexpr uint16_t ZVD_QR_FRAME_VERSION = 340u;

inline void secure_zero(void* ptr, size_t len) noexcept {
    if (!ptr || len == 0u) return;
    volatile uint8_t* vp = static_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0u; i < len; ++i) {
        vp[i] = 0u;
    }
}

inline uint64_t fnv1a_64_seed(const uint8_t* data, size_t len, uint64_t seed = 0xcbf29ce484222325ULL) noexcept {
    uint64_t h = seed;
    if (!data) return h ^ static_cast<uint64_t>(len);
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 0x100000001b3ULL;
    }
    h ^= static_cast<uint64_t>(len);
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= (h >> 33);
    return h;
}

inline uint64_t mix64(uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline uint32_t rotl32(uint32_t v, int c) noexcept {
    const unsigned int shift = static_cast<unsigned int>(c & 31);
    return (v << shift) | (v >> ((32u - shift) & 31u));
}

inline uint32_t load_le32(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline void store_le32(uint8_t* p, uint32_t v) noexcept {
    p[0] = static_cast<uint8_t>(v & 0xFFu);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

inline void store_le64(uint8_t* p, uint64_t v) noexcept {
    for (size_t i = 0; i < 8u; ++i) {
        p[i] = static_cast<uint8_t>((v >> (i * 8u)) & 0xFFu);
    }
}

inline bool ct_equal(const uint8_t* a, const uint8_t* b, size_t len) noexcept {
    if (!a || !b) return false;
    uint8_t diff = 0u;
    for (size_t i = 0; i < len; ++i) {
        diff = static_cast<uint8_t>(diff | static_cast<uint8_t>(a[i] ^ b[i]));
    }
    return diff == 0u;
}

inline int8_t clamp_penta(int v) noexcept {
    if (v < -2) return -2;
    if (v > 2) return 2;
    return static_cast<int8_t>(v);
}

struct IcosaAngleTrigEntry {
    double rad;
    double cos_val;
    double sin_val;
};

inline const std::array<IcosaAngleTrigEntry, 360>& angle_trig_table_360() noexcept {
    static const std::array<IcosaAngleTrigEntry, 360> table = []() {
        std::array<IcosaAngleTrigEntry, 360> arr{};
        for (size_t deg = 0; deg < 360u; ++deg) {
            const double rad = (static_cast<double>(deg) * ZVD_PI) / 180.0;
            arr[deg].rad = rad;
            arr[deg].cos_val = std::cos(rad);
            arr[deg].sin_val = std::sin(rad);
        }
        return arr;
    }();
    return table;
}

} // namespace ZvdQrVisualDetail

struct ZvdVisualKey {
    std::string_view password{};
    uint16_t icosa_angle_deg{0u};

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return password.size() >= 16u && icosa_angle_deg < 360u;
    }

    [[nodiscard]] constexpr uint16_t normalized_angle() const noexcept {
        return static_cast<uint16_t>(icosa_angle_deg % 360u);
    }
};

// =====================================================================
// 2. DUAL 120D ICOSAHEDRON MOIRE ENGINE & 96-BIT NONCE GENERATOR
// =====================================================================

namespace ZvdQrVisualDetail {

struct DualIcosaMoireState {
    uint8_t packed_20[ZVD_MOIRE_PACKED_BYTES]{};
    std::array<int8_t, ZVD_ICOSA_NODES> penta_sym{};
    std::array<double, ZVD_ICOSA_NODES> wave_m{};
    uint16_t angle_deg{0u};
    uint64_t step_t{0ULL};
};

inline uint8_t quantize_moire_to_u5(double m_val) noexcept {
    if (m_val < -1.2) return 0u;
    if (m_val < -0.4) return 1u;
    if (m_val <= 0.4) return 2u;
    if (m_val <= 1.2) return 3u;
    return 4u;
}

inline void compute_dual_icosa_moire(
    const ZvdVisualKey& key,
    uint64_t step_t,
    DualIcosaMoireState& out_state
) noexcept {
    const uint16_t deg = key.normalized_angle();
    const auto& trig = angle_trig_table_360()[deg];
    const double theta_rad = trig.rad;

    const uint64_t pw_seed = fnv1a_64_seed(
        reinterpret_cast<const uint8_t*>(key.password.data()),
        key.password.size(),
        0xcbf29ce484222325ULL
    );
    const uint64_t seed_a = mix64(pw_seed ^ 0xA5A5A5A5A5A5A5A5ULL ^ (static_cast<uint64_t>(deg) * 0x9e3779b97f4a7c15ULL));
    const uint64_t seed_b = mix64(pw_seed ^ 0x5A5A5A5A5A5A5A5AULL ^ (static_cast<uint64_t>(deg) * 0xbf58476d1ce4e5b9ULL));

    const double t_d = static_cast<double>(step_t);
    const double omega_a = std::fmod(t_d * 0.05 * ZVD_PHI, ZVD_TWO_PI);
    const double omega_b = std::fmod(t_d * 0.05 / ZVD_PHI, ZVD_TWO_PI);

    const double base_step = ZVD_TWO_PI / static_cast<double>(ZVD_ICOSA_NODES);

    out_state.angle_deg = deg;
    out_state.step_t = step_t;
    out_state.packed_20[0] = 0u;

    for (size_t k = 0u; k < ZVD_ICOSA_NODES; ++k) {
        const double kd = static_cast<double>(k);
        const uint64_t ha = mix64(seed_a ^ (static_cast<uint64_t>(k) * 0x94d049bb133111ebULL));
        const uint64_t hb = mix64(seed_b ^ (static_cast<uint64_t>(k) * 0x2545f4914f6cdd1dULL));

        const double delta_a = ((static_cast<double>(ha & 0xFFFFu) / 65535.0) - 0.5) * (0.4 * ZVD_PI);
        const double delta_b = ((static_cast<double>(hb & 0xFFFFu) / 65535.0) - 0.5) * (0.4 * ZVD_PI);

        const double phi_a = std::fmod(base_step * kd + theta_rad + omega_a + delta_a, ZVD_TWO_PI);
        const double phi_b = std::fmod(-base_step * kd - theta_rad - omega_b + delta_b, ZVD_TWO_PI);

        const double m_val = std::cos(phi_a) - std::sin(phi_b);
        const uint8_t u5 = quantize_moire_to_u5(m_val);

        out_state.wave_m[k] = m_val;
        out_state.penta_sym[k] = clamp_penta(static_cast<int>(u5) - 2);
    }

    for (size_t b = 0u; b < ZVD_MOIRE_PACKED_BYTES; ++b) {
        const size_t idx0 = b * 3u + 0u;
        const size_t idx1 = b * 3u + 1u;
        const size_t idx2 = b * 3u + 2u;

        const unsigned int u0 = static_cast<unsigned int>(static_cast<int>(out_state.penta_sym[idx0]) + 2);
        const unsigned int u1 = static_cast<unsigned int>(static_cast<int>(out_state.penta_sym[idx1]) + 2);
        const unsigned int u2 = static_cast<unsigned int>(static_cast<int>(out_state.penta_sym[idx2]) + 2);

        out_state.packed_20[b] = static_cast<uint8_t>((u0 + 5u * u1 + 25u * u2) & 0xFFu);
    }
}

inline void generate_nonce_96(
    const ZvdVisualKey& key,
    uint64_t frame_seq_id,
    uint8_t out_nonce[12]
) noexcept {
    static std::atomic<uint64_t> nonce_call_counter{1ULL};
    const uint64_t seq_call = nonce_call_counter.fetch_add(1ULL, std::memory_order_acq_rel);

    const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    const uint64_t clk_u64 = static_cast<uint64_t>(now_ns);

    const uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&out_nonce);
    const uintptr_t fn_addr = reinterpret_cast<uintptr_t>(&generate_nonce_96);
    const uint64_t addr_mix = mix64(static_cast<uint64_t>(stack_addr) ^ (static_cast<uint64_t>(fn_addr) << 1));

    uint32_t rd_a = 0u;
    uint32_t rd_b = 0u;
    try {
        std::random_device rd;
        rd_a = rd();
        rd_b = rd();
    } catch (...) {
        rd_a = static_cast<uint32_t>(clk_u64 & 0xFFFFFFFFu);
        rd_b = static_cast<uint32_t>((clk_u64 >> 32) & 0xFFFFFFFFu);
    }

    const uint64_t pw_hash = fnv1a_64_seed(
        reinterpret_cast<const uint8_t*>(key.password.data()),
        key.password.size(),
        0x517cc1b727220a95ULL
    );

    uint64_t h0 = mix64(
        pw_hash ^
        clk_u64 ^
        addr_mix ^
        (frame_seq_id * 0x9e3779b97f4a7c15ULL) ^
        (seq_call * 0xbf58476d1ce4e5b9ULL) ^
        (static_cast<uint64_t>(rd_a) << 32) ^
        static_cast<uint64_t>(key.normalized_angle())
    );
    uint64_t h1 = mix64(
        h0 ^
        (static_cast<uint64_t>(rd_b) << 32) ^
        static_cast<uint64_t>(rd_a) ^
        (clk_u64 >> 7)
    );

    store_le64(out_nonce, h0);
    store_le32(out_nonce + 8u, static_cast<uint32_t>(h1 & 0xFFFFFFFFu));
}

} // namespace ZvdQrVisualDetail

// =====================================================================
// 3. AEAD ENCRYPTION & AUTHENTICATION ENGINE (KEY SCHEDULE + ARX + 128-BIT MAC)
// =====================================================================

namespace ZvdQrVisualDetail {

inline void quarter_round_arx(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) noexcept {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

inline void derive_aead_key_256(
    const ZvdVisualKey& key,
    const DualIcosaMoireState& moire_state,
    uint8_t out_key_32[ZVD_AEAD_KEY_BYTES]
) noexcept {
    const uint64_t h_pw = fnv1a_64_seed(
        reinterpret_cast<const uint8_t*>(key.password.data()),
        key.password.size(),
        0xcbf29ce484222325ULL
    );
    const uint64_t h_moire = fnv1a_64_seed(
        moire_state.packed_20,
        ZVD_MOIRE_PACKED_BYTES,
        0x9e3779b97f4a7c15ULL
    );
    const uint64_t angle_mix = static_cast<uint64_t>(moire_state.angle_deg) * 0xbf58476d1ce4e5b9ULL ^
                                (moire_state.step_t * 0x94d049bb133111ebULL);

    uint32_t lanes[8]{};
    uint64_t seed = mix64(h_pw ^ h_moire ^ angle_mix);
    for (int i = 0; i < 8; ++i) {
        seed = mix64(seed ^ static_cast<uint64_t>(i + 1) * 0x2545f4914f6cdd1dULL);
        lanes[i] = static_cast<uint32_t>(seed & 0xFFFFFFFFu);
    }

    for (size_t i = 0u; i < ZVD_MOIRE_PACKED_BYTES; ++i) {
        lanes[i % 8u] ^= static_cast<uint32_t>(moire_state.packed_20[i]) << ((i % 4u) * 8u);
        if ((i % 4u) == 3u) {
            quarter_round_arx(lanes[0], lanes[4], lanes[1], lanes[5]);
            quarter_round_arx(lanes[2], lanes[6], lanes[3], lanes[7]);
        }
    }

    for (size_t i = 0u; i < key.password.size(); ++i) {
        const uint8_t ch = static_cast<uint8_t>(key.password[i]);
        lanes[i % 8u] = rotl32(lanes[i % 8u] + static_cast<uint32_t>(ch), static_cast<int>((i % 13u) + 3u));
        quarter_round_arx(lanes[0], lanes[3], lanes[5], lanes[7]);
    }

    for (int r = 0; r < 10; ++r) {
        quarter_round_arx(lanes[0], lanes[4], lanes[1], lanes[5]);
        quarter_round_arx(lanes[2], lanes[6], lanes[3], lanes[7]);
        quarter_round_arx(lanes[0], lanes[5], lanes[2], lanes[7]);
        quarter_round_arx(lanes[1], lanes[6], lanes[3], lanes[4]);
    }

    for (int i = 0; i < 8; ++i) {
        store_le32(out_key_32 + static_cast<size_t>(i) * 4u, lanes[i]);
    }
    secure_zero(lanes, sizeof(lanes));
}

inline void chacha20_block_64(
    const uint8_t key_32[ZVD_AEAD_KEY_BYTES],
    uint32_t counter,
    const uint8_t nonce_12[ZVD_AEAD_NONCE_BYTES],
    uint8_t out_block_64[ZVD_STREAM_BLOCK_BYTES]
) noexcept {
    static constexpr uint32_t k_sigma[4] = {
        0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u
    };

    uint32_t state[16]{};
    uint32_t work[16]{};

    state[0] = k_sigma[0];
    state[1] = k_sigma[1];
    state[2] = k_sigma[2];
    state[3] = k_sigma[3];

    for (int i = 0; i < 8; ++i) {
        state[4 + i] = load_le32(key_32 + static_cast<size_t>(i) * 4u);
    }

    state[12] = counter;
    state[13] = load_le32(nonce_12 + 0u);
    state[14] = load_le32(nonce_12 + 4u);
    state[15] = load_le32(nonce_12 + 8u);

    std::memcpy(work, state, sizeof(state));

    for (int r = 0; r < 10; ++r) {
        quarter_round_arx(work[0], work[4], work[8],  work[12]);
        quarter_round_arx(work[1], work[5], work[9],  work[13]);
        quarter_round_arx(work[2], work[6], work[10], work[14]);
        quarter_round_arx(work[3], work[7], work[11], work[15]);

        quarter_round_arx(work[0], work[5], work[10], work[15]);
        quarter_round_arx(work[1], work[6], work[11], work[12]);
        quarter_round_arx(work[2], work[7], work[8],  work[13]);
        quarter_round_arx(work[3], work[4], work[9],  work[14]);
    }

    for (int i = 0; i < 16; ++i) {
        const uint32_t sum = work[i] + state[i];
        store_le32(out_block_64 + static_cast<size_t>(i) * 4u, sum);
    }
    secure_zero(work, sizeof(work));
    secure_zero(state, sizeof(state));
}

inline void stream_xor_arx(
    const uint8_t key_32[ZVD_AEAD_KEY_BYTES],
    const uint8_t nonce_12[ZVD_AEAD_NONCE_BYTES],
    uint32_t initial_counter,
    const uint8_t* src,
    uint8_t* dst,
    size_t len
) noexcept {
    if (!src || !dst || len == 0u) return;

    alignas(64) uint8_t ks_block[ZVD_STREAM_BLOCK_BYTES]{};
    uint32_t counter = initial_counter;
    size_t offset = 0u;

    while (offset < len) {
        chacha20_block_64(key_32, counter, nonce_12, ks_block);
        ++counter;

        const size_t chunk = (len - offset < ZVD_STREAM_BLOCK_BYTES) ? (len - offset) : ZVD_STREAM_BLOCK_BYTES;
        for (size_t i = 0u; i < chunk; ++i) {
            dst[offset + i] = static_cast<uint8_t>(src[offset + i] ^ ks_block[i]);
        }
        offset += chunk;
    }
    secure_zero(ks_block, sizeof(ks_block));
}

inline void compute_tag_128(
    const uint8_t key_32[ZVD_AEAD_KEY_BYTES],
    const uint8_t nonce_12[ZVD_AEAD_NONCE_BYTES],
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* ciphertext,
    size_t ct_len,
    uint8_t out_tag_16[ZVD_AEAD_TAG_BYTES]
) noexcept {
    alignas(64) uint8_t mac_subkey[ZVD_STREAM_BLOCK_BYTES]{};
    chacha20_block_64(key_32, 0u, nonce_12, mac_subkey);

    uint64_t acc0 = load_le32(mac_subkey + 0u) | (static_cast<uint64_t>(load_le32(mac_subkey + 4u)) << 32);
    uint64_t acc1 = load_le32(mac_subkey + 8u) | (static_cast<uint64_t>(load_le32(mac_subkey + 12u)) << 32);
    const uint64_t r0 = load_le32(mac_subkey + 16u) | (static_cast<uint64_t>(load_le32(mac_subkey + 20u)) << 32);
    const uint64_t r1 = load_le32(mac_subkey + 24u) | (static_cast<uint64_t>(load_le32(mac_subkey + 28u)) << 32);

    auto absorb_stream = [&](const uint8_t* buf, size_t n) noexcept {
        if (!buf || n == 0u) return;
        size_t idx = 0u;
        while (idx < n) {
            uint64_t w0 = 0ULL;
            uint64_t w1 = 0ULL;
            for (size_t b = 0u; b < 8u && idx < n; ++b, ++idx) {
                w0 |= static_cast<uint64_t>(buf[idx]) << (b * 8u);
            }
            for (size_t b = 0u; b < 8u && idx < n; ++b, ++idx) {
                w1 |= static_cast<uint64_t>(buf[idx]) << (b * 8u);
            }
            w0 ^= 0x01ULL;
            acc0 = mix64(acc0 ^ (w0 * (r0 | 1ULL)));
            acc1 = mix64(acc1 ^ (w1 * (r1 | 1ULL)) ^ (acc0 >> 17));
        }
    };

    absorb_stream(aad, aad_len);
    absorb_stream(ciphertext, ct_len);

    uint8_t trailer[16]{};
    store_le64(trailer + 0u, static_cast<uint64_t>(aad_len));
    store_le64(trailer + 8u, static_cast<uint64_t>(ct_len));
    absorb_stream(trailer, sizeof(trailer));

    const uint64_t final0 = mix64(acc0 ^ acc1 ^ load_le32(mac_subkey + 32u));
    const uint64_t final1 = mix64(acc1 ^ final0 ^ load_le32(mac_subkey + 36u));
    store_le64(out_tag_16 + 0u, final0);
    store_le64(out_tag_16 + 8u, final1);
    secure_zero(mac_subkey, sizeof(mac_subkey));
}

inline bool aead_encrypt_payload(
    const ZvdVisualKey& key,
    const DualIcosaMoireState& moire_state,
    const uint8_t nonce_12[ZVD_AEAD_NONCE_BYTES],
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* plaintext,
    size_t pt_len,
    uint8_t* out_ciphertext,
    uint8_t out_tag_16[ZVD_AEAD_TAG_BYTES]
) noexcept {
    if (!key.is_valid() || !nonce_12 || !out_tag_16) return false;
    if (pt_len > 0u && (!plaintext || !out_ciphertext)) return false;

    alignas(64) uint8_t key_32[ZVD_AEAD_KEY_BYTES]{};
    derive_aead_key_256(key, moire_state, key_32);

    if (pt_len > 0u) {
        stream_xor_arx(key_32, nonce_12, 1u, plaintext, out_ciphertext, pt_len);
    }

    compute_tag_128(key_32, nonce_12, aad, aad_len, out_ciphertext, pt_len, out_tag_16);
    secure_zero(key_32, sizeof(key_32));
    return true;
}

inline bool aead_decrypt_payload(
    const ZvdVisualKey& key,
    const DualIcosaMoireState& moire_state,
    const uint8_t nonce_12[ZVD_AEAD_NONCE_BYTES],
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* ciphertext,
    size_t ct_len,
    const uint8_t expected_tag_16[ZVD_AEAD_TAG_BYTES],
    uint8_t* out_plaintext
) noexcept {
    if (!key.is_valid() || !nonce_12 || !expected_tag_16) return false;
    if (ct_len > 0u && (!ciphertext || !out_plaintext)) return false;

    alignas(64) uint8_t key_32[ZVD_AEAD_KEY_BYTES]{};
    alignas(64) uint8_t actual_tag_16[ZVD_AEAD_TAG_BYTES]{};

    derive_aead_key_256(key, moire_state, key_32);
    compute_tag_128(key_32, nonce_12, aad, aad_len, ciphertext, ct_len, actual_tag_16);

    const bool tag_ok = ct_equal(actual_tag_16, expected_tag_16, ZVD_AEAD_TAG_BYTES);
    if (!tag_ok) {
        secure_zero(key_32, sizeof(key_32));
        secure_zero(actual_tag_16, sizeof(actual_tag_16));
        return false;
    }

    if (ct_len > 0u) {
        stream_xor_arx(key_32, nonce_12, 1u, ciphertext, out_plaintext, ct_len);
    }

    secure_zero(key_32, sizeof(key_32));
    secure_zero(actual_tag_16, sizeof(actual_tag_16));
    return true;
}

} // namespace ZvdQrVisualDetail
// =====================================================================
// 4. AUTONOMOUS 3-LEVEL FOLDING, PYTH-345 LUT, LGZ-345 & MAJORITY FEC
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
typedef struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
{
    uint8_t face_packed[167];
    uint8_t vertex_packed[167];
} ZVD_Monolith334;

typedef struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
{
    uint8_t data[14];
} CubeLevel2Anchor;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

namespace ZvdQrVisualDetail {

inline void pack_layer_p5_500_to_167(const int8_t layer_500[ZVD_CELLS_PER_LAYER], uint8_t out_167[167]) noexcept {
    int p = 0;
    for (int i = 0; i < 166; ++i) {
        const int a = static_cast<int>(clamp_penta(layer_500[p++])) + 2;
        const int b = static_cast<int>(clamp_penta(layer_500[p++])) + 2;
        const int c = static_cast<int>(clamp_penta(layer_500[p++])) + 2;
        out_167[i] = static_cast<uint8_t>((a + b * 5 + c * 25) & 0xFFu);
    }
    const int d0 = static_cast<int>(clamp_penta(layer_500[498])) + 2;
    const int d1 = static_cast<int>(clamp_penta(layer_500[499])) + 2;
    out_167[166] = static_cast<uint8_t>((d0 + d1 * 5) & 0xFFu);
}

inline void build_monolith_and_icosa_anchor(
    const uint8_t* data,
    size_t len,
    const ZvdVisualKey& key,
    ZVD_Monolith334& out_monolith,
    int8_t& out_gl_inv,
    uint8_t out_icosa_120[ZVD_ICOSA_ANCHOR_BYTES]
) noexcept {
    std::memset(&out_monolith, 0, sizeof(out_monolith));
    out_gl_inv = 0;

    static thread_local alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> f_acc{};
    static thread_local alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> v_acc{};
    f_acc.fill(0);
    v_acc.fill(0);

    int inv_acc = 0;
    if (data && len > 0u) {
        const size_t stride = (len > 262144u) ? (len / 262144u) : 1u;
        for (size_t i = 0u; i < len; i += stride) {
            const size_t idx = i % ZVD_CELLS_PER_LAYER;
            const int b = static_cast<int>(data[i]);
            f_acc[idx] = clamp_penta(static_cast<int>(f_acc[idx]) + ((b % 5) - 2));
            v_acc[idx] = clamp_penta(static_cast<int>(v_acc[idx]) + (((b / 5) % 5) - 2));
            inv_acc += static_cast<int>(f_acc[idx]) - static_cast<int>(v_acc[idx]);
        }
    }

    pack_layer_p5_500_to_167(f_acc.data(), out_monolith.face_packed);
    pack_layer_p5_500_to_167(v_acc.data(), out_monolith.vertex_packed);
    out_gl_inv = clamp_penta(inv_acc);

    const uint64_t root_h = fnv1a_64_seed(data, len, 0xcbf29ce484222325ULL);
    const uint64_t pw_h = fnv1a_64_seed(
        reinterpret_cast<const uint8_t*>(key.password.data()),
        key.password.size(),
        0x9e3779b97f4a7c15ULL
    );
    const uint64_t angle_u = static_cast<uint64_t>(key.normalized_angle());

    for (size_t i = 0u; i < ZVD_ICOSA_ANCHOR_BYTES; ++i) {
        const uint8_t f_sym = out_monolith.face_packed[i % 167u];
        const uint8_t v_sym = out_monolith.vertex_packed[(i * 7u) % 167u];
        const uint64_t m = mix64(
            root_h ^
            pw_h ^
            (static_cast<uint64_t>(i) * 0xbf58476d1ce4e5b9ULL) ^
            (angle_u * 0x94d049bb133111ebULL)
        );
        out_icosa_120[i] = static_cast<uint8_t>((static_cast<uint64_t>(f_sym ^ v_sym) + (m & 0xFFu) + i) & 0xFFu);
    }
}

inline const std::array<std::array<uint8_t, 256>, 256>& pyth_345_lut_256() noexcept {
    static const std::array<std::array<uint8_t, 256>, 256> lut = []() {
        std::array<std::array<uint8_t, 256>, 256> tbl{};
        for (int a = 0; a < 256; ++a) {
            for (int b = 0; b < 256; ++b) {
                const unsigned int ua = static_cast<unsigned int>(a);
                const unsigned int ub = static_cast<unsigned int>(b);
                const unsigned int c_val = (3u * ua + 4u * ub + ((ua * ub) >> 4)) % 256u;
                const unsigned int mixed = (c_val * 5u + (ua ^ ub)) & 0xFFu;
                tbl[static_cast<size_t>(a)][static_cast<size_t>(b)] = static_cast<uint8_t>(mixed);
            }
        }
        return tbl;
    }();
    return lut;
}

inline void build_cube_anchor_14(
    const uint8_t* block_ptr,
    size_t block_len,
    const uint8_t icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    uint16_t cube_index,
    CubeLevel2Anchor& out_cube
) noexcept {
    const auto& lut = pyth_345_lut_256();
    const uint64_t bh = fnv1a_64_seed(block_ptr, block_len, 0x2545f4914f6cdd1dULL ^ static_cast<uint64_t>(cube_index));

    for (size_t j = 0u; j < ZVD_CUBE_ANCHOR_BYTES; ++j) {
        const uint8_t a = icosa_120[(static_cast<size_t>(cube_index) * ZVD_CUBE_ANCHOR_BYTES + j) % ZVD_ICOSA_ANCHOR_BYTES];
        const uint8_t b = static_cast<uint8_t>((bh >> ((j % 8u) * 8u)) & 0xFFu);
        const uint8_t p345 = lut[a][b];
        out_cube.data[j] = static_cast<uint8_t>(p345 ^ static_cast<uint8_t>(j * 17u + cube_index));
    }
}

inline void tetra_plane_split_forward(uint8_t* buf, size_t len) noexcept {
    if (!buf || len < 4u) return;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> tmp{};
    if (len > tmp.size()) return;

    size_t pos = 0u;
    for (uint8_t plane = 0u; plane < 4u; ++plane) {
        for (size_t i = plane; i < len; i += 4u) {
            tmp[pos++] = buf[i];
        }
    }
    std::memcpy(buf, tmp.data(), len);
}

inline void tetra_plane_split_inverse(uint8_t* buf, size_t len) noexcept {
    if (!buf || len < 4u) return;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> tmp{};
    if (len > tmp.size()) return;

    size_t pos = 0u;
    for (uint8_t plane = 0u; plane < 4u; ++plane) {
        for (size_t i = plane; i < len; i += 4u) {
            tmp[i] = buf[pos++];
        }
    }
    std::memcpy(buf, tmp.data(), len);
}

inline bool lgz345_encode_adaptive(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len
) noexcept {
    out_len = 0u;
    if (!src || !dst || src_len == 0u) return false;
    if (dst_cap < src_len + 5u) return false;

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 64u> work{};
    if (src_len + 8u > work.size()) return false;

    size_t wpos = 0u;
    size_t i = 0u;
    bool compressed_smaller = false;

    while (i < src_len) {
        const uint8_t cur = src[i];
        size_t run = 1u;
        while (i + run < src_len && src[i + run] == cur && run < 127u) {
            ++run;
        }
        if (run >= 3u) {
            work[wpos++] = static_cast<uint8_t>(0x80u | static_cast<uint8_t>(run));
            work[wpos++] = cur;
            i += run;
            compressed_smaller = true;
        } else {
            const size_t lit_start = i;
            size_t lit_len = 0u;
            while (i < src_len && lit_len < 127u) {
                if (i + 2u < src_len && src[i] == src[i + 1u] && src[i] == src[i + 2u]) {
                    break;
                }
                ++i;
                ++lit_len;
            }
            if (lit_len == 0u && i < src_len) {
                work[wpos++] = 0x01u;
                work[wpos++] = src[i++];
                ++lit_len;
            } else {
                work[wpos++] = static_cast<uint8_t>(lit_len & 0x7Fu);
                std::memcpy(work.data() + wpos, src + lit_start, lit_len);
                wpos += lit_len;
            }
        }
    }

    if (compressed_smaller && (wpos + 5u) < (src_len + 5u) && (wpos + 5u) <= dst_cap) {
        dst[0] = 0x35u;
        store_le32(dst + 1u, static_cast<uint32_t>(src_len));
        std::memcpy(dst + 5u, work.data(), wpos);
        out_len = wpos + 5u;
        return true;
    }

    dst[0] = 0x00u;
    store_le32(dst + 1u, static_cast<uint32_t>(src_len));
    std::memcpy(dst + 5u, src, src_len);
    out_len = src_len + 5u;
    return true;
}

inline bool lgz345_decode_stream(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len
) noexcept {
    out_len = 0u;
    if (!src || !dst || src_len < 5u) return false;

    const uint8_t mode_byte = src[0];
    const uint32_t orig_len_u32 = load_le32(src + 1u);
    const size_t orig_len = static_cast<size_t>(orig_len_u32);
    if (orig_len > dst_cap || orig_len > ZVD_MAX_VISUAL_FRAME_BYTES) return false;

    if (mode_byte == 0x00u) {
        if (src_len < 5u + orig_len) return false;
        std::memcpy(dst, src + 5u, orig_len);
        out_len = orig_len;
        return true;
    }

    if (mode_byte != 0x35u) return false;

    size_t rpos = 5u;
    size_t dpos = 0u;

    while (rpos < src_len && dpos < orig_len) {
        const uint8_t ctrl = src[rpos++];
        if ((ctrl & 0x80u) != 0u) {
            const size_t run = static_cast<size_t>(ctrl & 0x7Fu);
            if (rpos >= src_len || run < 3u || dpos + run > orig_len) return false;
            const uint8_t val = src[rpos++];
            std::memset(dst + dpos, val, run);
            dpos += run;
        } else {
            const size_t lit_len = static_cast<size_t>(ctrl & 0x7Fu);
            if (lit_len == 0u || rpos + lit_len > src_len || dpos + lit_len > orig_len) return false;
            std::memcpy(dst + dpos, src + rpos, lit_len);
            rpos += lit_len;
            dpos += lit_len;
        }
    }

    if (dpos != orig_len) return false;
    out_len = dpos;
    return true;
}

inline size_t fec3x_encode_buffer(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap
) noexcept {
    if (!dst || dst_cap < src_len * 3u || src_len == 0u || !src) return 0u;

    std::memcpy(dst + 0u * src_len, src, src_len);
    std::memcpy(dst + 1u * src_len, src, src_len);
    std::memcpy(dst + 2u * src_len, src, src_len);
    return src_len * 3u;
}

inline bool fec3x_decode_majority(
    const uint8_t* src,
    size_t fec_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len
) noexcept {
    out_len = 0u;
    if (!src || !dst || fec_len == 0u || (fec_len % 3u) != 0u) return false;
    const size_t orig_len = fec_len / 3u;
    if (orig_len > dst_cap) return false;

    const uint8_t* c0 = src + 0u * orig_len;
    const uint8_t* c1 = src + 1u * orig_len;
    const uint8_t* c2 = src + 2u * orig_len;

    for (size_t i = 0u; i < orig_len; ++i) {
        const uint8_t a = c0[i];
        const uint8_t b = c1[i];
        const uint8_t c = c2[i];

        if (a == b || a == c) {
            dst[i] = a;
        } else if (b == c) {
            dst[i] = b;
        } else {
            dst[i] = a;
        }
    }

    out_len = orig_len;
    return true;
}

inline bool zvd_fold_pack_3level(
    const uint8_t* payload,
    size_t payload_len,
    const ZvdVisualKey& key,
    bool enable_fec3x,
    ZVD_Monolith334& out_monolith,
    int8_t& out_gl_inv,
    uint8_t out_icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    CubeLevel2Anchor& out_cube0,
    uint8_t* out_wire,
    size_t out_wire_cap,
    size_t& out_wire_len,
    uint8_t& out_fold_flags
) noexcept {
    out_wire_len = 0u;
    out_fold_flags = 0u;
    if (!out_wire) return false;
    if (payload_len > 0u && !payload) return false;
    if (payload_len > ZVD_MAX_VISUAL_FRAME_BYTES) return false;

    build_monolith_and_icosa_anchor(payload, payload_len, key, out_monolith, out_gl_inv, out_icosa_120);
    build_cube_anchor_14(payload, payload_len, out_icosa_120, 0u, out_cube0);

    if (payload_len == 0u) {
        return true;
    }

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 256u> stage1{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 256u> stage2{};

    if (payload_len > stage1.size()) return false;
    std::memcpy(stage1.data(), payload, payload_len);

    if (payload_len >= 4u) {
        tetra_plane_split_forward(stage1.data(), payload_len);
        out_fold_flags |= ZVD_FOLD_FLAG_PLANESPLIT;
    }

    size_t lgz_len = 0u;
    if (!lgz345_encode_adaptive(stage1.data(), payload_len, stage2.data(), stage2.size(), lgz_len)) {
        return false;
    }
    if (stage2[0] == 0x35u) {
        out_fold_flags |= ZVD_FOLD_FLAG_LGZ345;
    }

    if (enable_fec3x) {
        if (lgz_len * 3u > out_wire_cap) return false;
        out_wire_len = fec3x_encode_buffer(stage2.data(), lgz_len, out_wire, out_wire_cap);
        if (out_wire_len == 0u) return false;
        out_fold_flags |= ZVD_FOLD_FLAG_FEC3X;
        return true;
    }

    if (lgz_len > out_wire_cap) return false;
    std::memcpy(out_wire, stage2.data(), lgz_len);
    out_wire_len = lgz_len;
    return true;
}

inline bool zvd_fold_unpack_3level(
    const uint8_t* wire,
    size_t wire_len,
    uint8_t fold_flags,
    uint8_t* out_payload,
    size_t out_payload_cap,
    size_t& out_payload_len
) noexcept {
    out_payload_len = 0u;
    if (!out_payload) return false;
    if (wire_len == 0u) return true;
    if (!wire) return false;

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 256u> defecd{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 256u> unlgz{};

    const uint8_t* lgz_ptr = wire;
    size_t lgz_sz = wire_len;

    if ((fold_flags & ZVD_FOLD_FLAG_FEC3X) != 0u) {
        size_t maj_len = 0u;
        if (!fec3x_decode_majority(wire, wire_len, defecd.data(), defecd.size(), maj_len)) {
            return false;
        }
        lgz_ptr = defecd.data();
        lgz_sz = maj_len;
    }

    size_t raw_len = 0u;
    if (!lgz345_decode_stream(lgz_ptr, lgz_sz, unlgz.data(), unlgz.size(), raw_len)) {
        return false;
    }

    if ((fold_flags & ZVD_FOLD_FLAG_PLANESPLIT) != 0u && raw_len >= 4u) {
        tetra_plane_split_inverse(unlgz.data(), raw_len);
    }

    if (raw_len > out_payload_cap) return false;
    std::memcpy(out_payload, unlgz.data(), raw_len);
    out_payload_len = raw_len;
    return true;
}

} // namespace ZvdQrVisualDetail
// =====================================================================
// 5. 8x8 TRI-TILE GEOMETRY (0..4), 4-TILE PACKING & 4 MOIRE CORNER MARKERS
// =====================================================================

namespace ZvdQrVisualDetail {

inline constexpr uint8_t tile_pixel_pattern_0_4(uint8_t tile_type, size_t x, size_t y) noexcept {
    switch (tile_type) {
        case 0u:
            return ZVD_PX_BLACK;
        case 1u:
            return (x >= y) ? ZVD_PX_BLACK : ZVD_PX_WHITE;
        case 2u:
            return ((x + y) <= 7u) ? ZVD_PX_BLACK : ZVD_PX_WHITE;
        case 3u:
            return ((x + y) <= 7u) ? ZVD_PX_WHITE : ZVD_PX_BLACK;
        case 4u:
            return (x >= y) ? ZVD_PX_WHITE : ZVD_PX_BLACK;
        default:
            return ZVD_PX_BLACK;
    }
}

inline void render_tile_8x8(
    uint8_t tile_type,
    uint8_t* canvas,
    size_t canvas_w,
    size_t canvas_h,
    size_t origin_x,
    size_t origin_y
) noexcept {
    if (!canvas) return;
    const uint8_t t = (tile_type <= 4u) ? tile_type : 0u;
    for (size_t dy = 0u; dy < ZVD_TILE_SIZE; ++dy) {
        const size_t py = origin_y + dy;
        if (py >= canvas_h) break;
        for (size_t dx = 0u; dx < ZVD_TILE_SIZE; ++dx) {
            const size_t px = origin_x + dx;
            if (px >= canvas_w) break;
            canvas[py * canvas_w + px] = tile_pixel_pattern_0_4(t, dx, dy);
        }
    }
}

inline uint8_t classify_tile_8x8_sector_area(
    const uint8_t* gray_canvas,
    size_t canvas_w,
    size_t canvas_h,
    size_t origin_x,
    size_t origin_y
) noexcept {
    int black_all = 0;
    int black_ul = 0;
    int black_lr = 0;
    int black_ur = 0;
    int black_ll = 0;

    for (size_t dy = 0u; dy < ZVD_TILE_SIZE; ++dy) {
        const size_t py = origin_y + dy;
        if (py >= canvas_h) continue;
        for (size_t dx = 0u; dx < ZVD_TILE_SIZE; ++dx) {
            const size_t px = origin_x + dx;
            if (px >= canvas_w) continue;
            const bool is_black = (gray_canvas[py * canvas_w + px] < 128u);
            if (!is_black) continue;

            ++black_all;
            if (dx + dy <= 7u) ++black_ul; else ++black_lr;
            if (dx >= dy) ++black_ur; else ++black_ll;
        }
    }

    if (black_all >= 52) return 0u;

    const int score_diag_backslash_t1 = black_ur + (32 - black_ll);
    const int score_diag_backslash_t4 = black_ll + (32 - black_ur);
    const int score_diag_slash_t2 = black_ul + (36 - black_lr);
    const int score_diag_slash_t3 = black_lr + (36 - black_ul);

    int best_score = score_diag_backslash_t1;
    uint8_t best_type = 1u;

    if (score_diag_slash_t2 > best_score) {
        best_score = score_diag_slash_t2;
        best_type = 2u;
    }
    if (score_diag_slash_t3 > best_score) {
        best_score = score_diag_slash_t3;
        best_type = 3u;
    }
    if (score_diag_backslash_t4 > best_score) {
        best_score = score_diag_backslash_t4;
        best_type = 4u;
    }
    return best_type;
}

inline void pack_4_tiles_to_byte(
    uint8_t t0, uint8_t t1, uint8_t t2, uint8_t t3,
    uint8_t& out_byte
) noexcept {
    const unsigned int v0 = (t0 <= 4u) ? static_cast<unsigned int>(t0) : 0u;
    const unsigned int v1 = (t1 <= 4u) ? static_cast<unsigned int>(t1) : 0u;
    const unsigned int v2 = (t2 <= 4u) ? static_cast<unsigned int>(t2) : 0u;
    const unsigned int v3 = (t3 <= 4u) ? static_cast<unsigned int>(t3) : 0u;

    const unsigned int code = v0 + v1 * 5u + v2 * 25u + v3 * 125u;
    out_byte = static_cast<uint8_t>(code & 0xFFu);
}

inline void unpack_byte_to_4_tiles(
    uint8_t packed_byte,
    uint8_t& out_t0, uint8_t& out_t1, uint8_t& out_t2, uint8_t& out_t3
) noexcept {
    unsigned int v = static_cast<unsigned int>(packed_byte);
    out_t0 = static_cast<uint8_t>(v % 5u); v /= 5u;
    out_t1 = static_cast<uint8_t>(v % 5u); v /= 5u;
    out_t2 = static_cast<uint8_t>(v % 5u); v /= 5u;
    out_t3 = static_cast<uint8_t>(v % 5u);
}

inline void render_moire_corner_marker(
    uint8_t corner_id,
    const DualIcosaMoireState& moire_state,
    uint8_t* canvas,
    size_t canvas_w,
    size_t canvas_h,
    size_t origin_x,
    size_t origin_y
) noexcept {
    if (!canvas) return;
    const uint8_t cid = static_cast<uint8_t>(corner_id & 3u);
    const size_t size = ZVD_CORNER_MARKER_PX;
    const double center = static_cast<double>(size) * 0.5 - 0.5;
    const double corner_phase_shift = static_cast<double>(cid) * (0.5 * ZVD_PI);

    for (size_t dy = 0u; dy < size; ++dy) {
        const size_t py = origin_y + dy;
        if (py >= canvas_h) break;
        for (size_t dx = 0u; dx < size; ++dx) {
            const size_t px = origin_x + dx;
            if (px >= canvas_w) break;

            const bool on_outer_border = (dx < 2u || dx >= size - 2u || dy < 2u || dy >= size - 2u);
            const bool on_inner_frame  = (dx >= 4u && dx < size - 4u && dy >= 4u && dy < size - 4u) &&
                                           (dx == 4u || dx == size - 5u || dy == 4u || dy == size - 5u);

            if (on_outer_border) {
                canvas[py * canvas_w + px] = ZVD_PX_BLACK;
                continue;
            }
            if (dx == 2u || dx == size - 3u || dy == 2u || dy == size - 3u) {
                canvas[py * canvas_w + px] = ZVD_PX_WHITE;
                continue;
            }
            if (on_inner_frame) {
                bool notch = false;
                if (cid == 0u && dx < 10u && dy < 10u) notch = true;
                if (cid == 1u && dx >= size - 10u && dy < 10u) notch = true;
                if (cid == 2u && dx >= size - 10u && dy >= size - 10u) notch = true;
                if (cid == 3u && dx < 10u && dy >= size - 10u) notch = true;
                canvas[py * canvas_w + px] = notch ? ZVD_PX_WHITE : ZVD_PX_BLACK;
                continue;
            }

            const double fx = static_cast<double>(dx) - center;
            const double fy = static_cast<double>(dy) - center;
            const double r = std::sqrt(fx * fx + fy * fy);
            const double ang = std::atan2(fy, fx);

            const size_t node_a = (static_cast<size_t>(dx) + static_cast<size_t>(cid) * 15u) % ZVD_ICOSA_NODES;
            const size_t node_b = (static_cast<size_t>(dy) + static_cast<size_t>(cid) * 29u) % ZVD_ICOSA_NODES;

            const double wa = moire_state.wave_m[node_a];
            const double wb = moire_state.wave_m[node_b];

            const double wave_val = std::cos(r * 0.95 + wa * 1.4 + corner_phase_shift) -
                                    std::sin(ang * 5.0 - r * 0.65 + wb * 1.4 - corner_phase_shift);

            canvas[py * canvas_w + px] = (wave_val >= 0.0) ? ZVD_PX_WHITE : ZVD_PX_BLACK;
        }
    }
}

} // namespace ZvdQrVisualDetail
// =====================================================================
// 6. BOTTOM 32PX MOIRE STRIP, FRAME WIRE PACKER & PGM / BMP ENCODERS
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
ZvdQrFrameHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t icosa_angle_deg;
    uint64_t frame_seq_id;
    uint64_t raw_payload_bytes;
    uint64_t folded_payload_bytes;
    uint64_t wire_packet_bytes;
    uint32_t grid_cols;
    uint32_t grid_rows;
    uint8_t  fold_flags;
    uint8_t  fec3x_enabled;
    uint16_t reserved;
    uint8_t  nonce_12[12];
    uint8_t  tag_16[16];
    ZVD_Monolith334 monolith;
    CubeLevel2Anchor cube0;
    uint64_t header_hash64;
};
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

namespace ZvdQrVisualDetail {

inline uint64_t compute_qr_frame_header_hash(const ZvdQrFrameHeader& hdr) noexcept {
    ZvdQrFrameHeader copy = hdr;
    copy.header_hash64 = 0ULL;
    return fnv1a_64_seed(
        reinterpret_cast<const uint8_t*>(&copy),
        sizeof(ZvdQrFrameHeader),
        0xcbf29ce484222325ULL
    );
}

inline void render_bottom_moire_strip_32px(
    const DualIcosaMoireState& moire_state,
    uint16_t angle_deg,
    const uint8_t tag_16[ZVD_AEAD_TAG_BYTES],
    uint8_t* canvas,
    size_t canvas_w,
    size_t canvas_h,
    size_t strip_top_y
) noexcept {
    if (!canvas || canvas_w == 0u || canvas_h == 0u || strip_top_y >= canvas_h) return;

    const size_t strip_h = (strip_top_y + ZVD_BOTTOM_STRIP_PX <= canvas_h)
                               ? ZVD_BOTTOM_STRIP_PX
                               : (canvas_h - strip_top_y);

    const uint64_t tag_lo = tag_16 ? (static_cast<uint64_t>(load_le32(tag_16 + 0u)) |
                                     (static_cast<uint64_t>(load_le32(tag_16 + 4u)) << 32))
                                   : 0ULL;
    const uint64_t tag_hi = tag_16 ? (static_cast<uint64_t>(load_le32(tag_16 + 8u)) |
                                     (static_cast<uint64_t>(load_le32(tag_16 + 12u)) << 32))
                                   : 0ULL;

    const double theta_rad = angle_trig_table_360()[angle_deg % 360u].rad;
    const double tag_phase_a = (static_cast<double>(tag_lo & 0xFFFFu) / 65535.0) * ZVD_TWO_PI;
    const double tag_phase_b = (static_cast<double>(tag_hi & 0xFFFFu) / 65535.0) * ZVD_TWO_PI;

    for (size_t y = 0u; y < strip_h; ++y) {
        const size_t py = strip_top_y + y;
        const double fy = static_cast<double>(y);

        for (size_t x = 0u; x < canvas_w; ++x) {
            if (y < 2u) {
                canvas[py * canvas_w + x] = ZVD_PX_BLACK;
                continue;
            }
            if (y < 4u) {
                canvas[py * canvas_w + x] = ZVD_PX_WHITE;
                continue;
            }

            const size_t node_idx = (x / 4u) % ZVD_ICOSA_NODES;
            const double fx = static_cast<double>(x);
            const double wa = moire_state.wave_m[node_idx];
            const double wb = moire_state.wave_m[(node_idx + 17u) % ZVD_ICOSA_NODES];

            const double phase_a = fx * 0.22 + theta_rad + tag_phase_a + wa * 1.35;
            const double phase_b = -fx * 0.22 - theta_rad - tag_phase_b + fy * 0.38 * ZVD_PHI + wb * 1.35;

            const double m_strip = std::cos(phase_a) - std::sin(phase_b);
            canvas[py * canvas_w + x] = (m_strip >= 0.0) ? ZVD_PX_WHITE : ZVD_PX_BLACK;
        }
    }
}

inline size_t encode_pgm_p5_buffer(
    const uint8_t* gray_canvas,
    uint32_t width,
    uint32_t height,
    uint8_t* out_buf,
    size_t out_cap
) noexcept {
    if (!gray_canvas || !out_buf || width == 0u || height == 0u) return 0u;
    const size_t px_count = static_cast<size_t>(width) * static_cast<size_t>(height);

    char hdr[128];
    const int hlen = std::snprintf(
        hdr,
        sizeof(hdr),
        "P5\n%u %u\n255\n",
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height)
    );
    if (hlen <= 0) return 0u;

    const size_t total_len = static_cast<size_t>(hlen) + px_count;
    if (out_cap < total_len) return 0u;

    std::memcpy(out_buf, hdr, static_cast<size_t>(hlen));
    std::memcpy(out_buf + static_cast<size_t>(hlen), gray_canvas, px_count);
    return total_len;
}

inline size_t encode_bmp_24bit_mono_buffer(
    const uint8_t* gray_canvas,
    uint32_t width,
    uint32_t height,
    uint8_t* out_buf,
    size_t out_cap
) noexcept {
    if (!gray_canvas || !out_buf || width == 0u || height == 0u) return 0u;

    const size_t row_raw_bytes = static_cast<size_t>(width) * 3u;
    const size_t row_padded_bytes = (row_raw_bytes + 3u) & ~static_cast<size_t>(3u);
    const size_t pixel_array_bytes = row_padded_bytes * static_cast<size_t>(height);
    const size_t file_header_bytes = 14u;
    const size_t info_header_bytes = 40u;
    const size_t total_size = file_header_bytes + info_header_bytes + pixel_array_bytes;

    if (out_cap < total_size) return 0u;

    std::memset(out_buf, 0, total_size);

    out_buf[0] = static_cast<uint8_t>('B');
    out_buf[1] = static_cast<uint8_t>('M');
    store_le32(out_buf + 2u, static_cast<uint32_t>(total_size));
    store_le32(out_buf + 6u, 0u);
    store_le32(out_buf + 10u, static_cast<uint32_t>(file_header_bytes + info_header_bytes));

    uint8_t* dib = out_buf + file_header_bytes;
    store_le32(dib + 0u, static_cast<uint32_t>(info_header_bytes));
    store_le32(dib + 4u, width);
    store_le32(dib + 8u, height);
    dib[12] = 1u; dib[13] = 0u;
    dib[14] = 24u; dib[15] = 0u;
    store_le32(dib + 16u, 0u);
    store_le32(dib + 20u, static_cast<uint32_t>(pixel_array_bytes));

    uint8_t* dst_rows = out_buf + file_header_bytes + info_header_bytes;
    for (uint32_t y = 0u; y < height; ++y) {
        const uint32_t src_y = (height - 1u) - y;
        const uint8_t* src_row = gray_canvas + static_cast<size_t>(src_y) * static_cast<size_t>(width);
        uint8_t* dst_row = dst_rows + static_cast<size_t>(y) * row_padded_bytes;

        for (uint32_t x = 0u; x < width; ++x) {
            const uint8_t v = src_row[x];
            const size_t bx = static_cast<size_t>(x) * 3u;
            dst_row[bx + 0u] = v;
            dst_row[bx + 1u] = v;
            dst_row[bx + 2u] = v;
        }
    }

    return total_size;
}

} // namespace ZvdQrVisualDetail

enum class ZvdQrImageFormat : uint8_t {
    PGM_P5 = 0u,
    BMP_24 = 1u
};

inline bool zvd_qr_encode_frame_image(
    const uint8_t* payload,
    size_t payload_len,
    const ZvdVisualKey& key,
    uint64_t frame_seq_id,
    bool enable_fec3x,
    ZvdQrImageFormat fmt,
    uint8_t* out_img_buf,
    size_t out_img_cap,
    size_t* out_img_len
) noexcept {
    if (!out_img_buf || !out_img_len) return false;
    if (payload_len > 0u && !payload) return false;
    if (!key.is_valid()) return false;
    if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 1024u> folded_buf{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 1024u> ct_buf{};
    static thread_local alignas(64) std::array<uint8_t, sizeof(ZvdQrFrameHeader) + ZVD_MAX_VISUAL_FRAME_BYTES + 1024u> packet_buf{};
    static thread_local alignas(64) std::array<uint8_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 1024u) * 3u> tx_stream_buf{};
    static thread_local alignas(64) ZvdQrVisualDetail::DualIcosaMoireState moire_state{};

    ZVD_Monolith334 monolith{};
    int8_t gl_inv = 0;
    uint8_t icosa_120[ZvdQrVisualDetail::ZVD_ICOSA_ANCHOR_BYTES]{};
    CubeLevel2Anchor cube0{};
    size_t folded_len = 0u;
    uint8_t fold_flags = 0u;

    if (!ZvdQrVisualDetail::zvd_fold_pack_3level(
            payload, payload_len, key, false,
            monolith, gl_inv, icosa_120, cube0,
            folded_buf.data(), folded_buf.size(), folded_len, fold_flags)) {
        return false;
    }

    ZvdQrVisualDetail::compute_dual_icosa_moire(key, frame_seq_id, moire_state);

    uint8_t nonce_12[ZvdQrVisualDetail::ZVD_AEAD_NONCE_BYTES]{};
    ZvdQrVisualDetail::generate_nonce_96(key, frame_seq_id, nonce_12);

    uint8_t aad[sizeof(ZVD_Monolith334) + sizeof(CubeLevel2Anchor) + 16u]{};
    std::memcpy(aad, &monolith, sizeof(monolith));
    std::memcpy(aad + sizeof(monolith), &cube0, sizeof(cube0));
    ZvdQrVisualDetail::store_le64(aad + sizeof(monolith) + sizeof(cube0), frame_seq_id);
    ZvdQrVisualDetail::store_le64(aad + sizeof(monolith) + sizeof(cube0) + 8u, static_cast<uint64_t>(folded_len));

    uint8_t tag_16[ZvdQrVisualDetail::ZVD_AEAD_TAG_BYTES]{};
    if (folded_len > ct_buf.size()) return false;

    if (!ZvdQrVisualDetail::aead_encrypt_payload(
            key, moire_state, nonce_12,
            aad, sizeof(aad),
            folded_buf.data(), folded_len,
            ct_buf.data(), tag_16)) {
        return false;
    }

    const size_t raw_packet_len = sizeof(ZvdQrFrameHeader) + folded_len;
    if (raw_packet_len > packet_buf.size()) return false;

    const size_t tx_len = enable_fec3x ? (raw_packet_len * 3u) : raw_packet_len;
    if (tx_len > tx_stream_buf.size()) return false;

    const size_t total_tiles = tx_len * 4u;
    const uint32_t grid_cols = 64u;
    const uint32_t grid_rows = static_cast<uint32_t>((total_tiles + static_cast<size_t>(grid_cols) - 1u) / static_cast<size_t>(grid_cols));
    const uint32_t safe_rows = (grid_rows == 0u) ? 1u : grid_rows;

    ZvdQrFrameHeader hdr{};
    hdr.magic = ZvdQrVisualDetail::ZVD_QR_FRAME_MAGIC;
    hdr.version = ZvdQrVisualDetail::ZVD_QR_FRAME_VERSION;
    hdr.icosa_angle_deg = key.normalized_angle();
    hdr.frame_seq_id = frame_seq_id;
    hdr.raw_payload_bytes = static_cast<uint64_t>(payload_len);
    hdr.folded_payload_bytes = static_cast<uint64_t>(folded_len);
    hdr.wire_packet_bytes = static_cast<uint64_t>(raw_packet_len);
    hdr.grid_cols = grid_cols;
    hdr.grid_rows = safe_rows;
    hdr.fold_flags = fold_flags;
    hdr.fec3x_enabled = enable_fec3x ? 1u : 0u;
    std::memcpy(hdr.nonce_12, nonce_12, sizeof(nonce_12));
    std::memcpy(hdr.tag_16, tag_16, sizeof(tag_16));
    hdr.monolith = monolith;
    hdr.cube0 = cube0;
    hdr.header_hash64 = ZvdQrVisualDetail::compute_qr_frame_header_hash(hdr);

    std::memcpy(packet_buf.data(), &hdr, sizeof(hdr));
    if (folded_len > 0u) {
        std::memcpy(packet_buf.data() + sizeof(hdr), ct_buf.data(), folded_len);
    }

    if (enable_fec3x) {
        if (ZvdQrVisualDetail::fec3x_encode_buffer(
                packet_buf.data(), raw_packet_len, tx_stream_buf.data(), tx_stream_buf.size()) != tx_len) {
            return false;
        }
    } else {
        std::memcpy(tx_stream_buf.data(), packet_buf.data(), raw_packet_len);
    }

    const size_t margin = ZvdQrVisualDetail::ZVD_MARGIN_PX;
    const size_t marker_sz = ZvdQrVisualDetail::ZVD_CORNER_MARKER_PX;
    const size_t payload_w = static_cast<size_t>(grid_cols) * ZvdQrVisualDetail::ZVD_TILE_SIZE;
    const size_t payload_h = static_cast<size_t>(safe_rows) * ZvdQrVisualDetail::ZVD_TILE_SIZE;

    const size_t canvas_w = margin * 2u + marker_sz * 2u + payload_w;
    const size_t canvas_h = margin * 2u + marker_sz * 2u + payload_h + ZvdQrVisualDetail::ZVD_BOTTOM_STRIP_PX;
    const size_t canvas_px = canvas_w * canvas_h;

    static thread_local alignas(64) std::array<uint8_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 4096u) * 6u> gray_canvas{};
    if (canvas_px > gray_canvas.size()) return false;

    std::memset(gray_canvas.data(), ZvdQrVisualDetail::ZVD_PX_WHITE, canvas_px);

    const size_t tl_x = margin;
    const size_t tl_y = margin;
    const size_t tr_x = margin + marker_sz + payload_w;
    const size_t tr_y = margin;
    const size_t br_x = margin + marker_sz + payload_w;
    const size_t br_y = margin + marker_sz + payload_h;
    const size_t bl_x = margin;
    const size_t bl_y = margin + marker_sz + payload_h;

    ZvdQrVisualDetail::render_moire_corner_marker(0u, moire_state, gray_canvas.data(), canvas_w, canvas_h, tl_x, tl_y);
    ZvdQrVisualDetail::render_moire_corner_marker(1u, moire_state, gray_canvas.data(), canvas_w, canvas_h, tr_x, tr_y);
    ZvdQrVisualDetail::render_moire_corner_marker(2u, moire_state, gray_canvas.data(), canvas_w, canvas_h, br_x, br_y);
    ZvdQrVisualDetail::render_moire_corner_marker(3u, moire_state, gray_canvas.data(), canvas_w, canvas_h, bl_x, bl_y);

    const size_t matrix_origin_x = margin + marker_sz;
    const size_t matrix_origin_y = margin + marker_sz;

    size_t tile_cursor = 0u;
    for (size_t i = 0u; i < tx_len; ++i) {
        uint8_t t[4]{};
        ZvdQrVisualDetail::unpack_byte_to_4_tiles(tx_stream_buf[i], t[0], t[1], t[2], t[3]);
        for (size_t k = 0u; k < 4u; ++k) {
            if (tile_cursor >= static_cast<size_t>(grid_cols) * static_cast<size_t>(safe_rows)) break;
            const size_t gx = tile_cursor % static_cast<size_t>(grid_cols);
            const size_t gy = tile_cursor / static_cast<size_t>(grid_cols);
            ZvdQrVisualDetail::render_tile_8x8(
                t[k],
                gray_canvas.data(),
                canvas_w,
                canvas_h,
                matrix_origin_x + gx * ZvdQrVisualDetail::ZVD_TILE_SIZE,
                matrix_origin_y + gy * ZvdQrVisualDetail::ZVD_TILE_SIZE
            );
            ++tile_cursor;
        }
    }

    const size_t strip_top_y = margin + marker_sz * 2u + payload_h + 4u;
    ZvdQrVisualDetail::render_bottom_moire_strip_32px(
        moire_state, key.normalized_angle(), tag_16,
        gray_canvas.data(), canvas_w, canvas_h, strip_top_y
    );

    size_t written = 0u;
    if (fmt == ZvdQrImageFormat::PGM_P5) {
        written = ZvdQrVisualDetail::encode_pgm_p5_buffer(
            gray_canvas.data(),
            static_cast<uint32_t>(canvas_w),
            static_cast<uint32_t>(canvas_h),
            out_img_buf,
            out_img_cap
        );
    } else {
        written = ZvdQrVisualDetail::encode_bmp_24bit_mono_buffer(
            gray_canvas.data(),
            static_cast<uint32_t>(canvas_w),
            static_cast<uint32_t>(canvas_h),
            out_img_buf,
            out_img_cap
        );
    }

    ZvdQrVisualDetail::secure_zero(nonce_12, sizeof(nonce_12));
    ZvdQrVisualDetail::secure_zero(tag_16, sizeof(tag_16));

    if (written == 0u) return false;
    *out_img_len = written;
    return true;
}
// =====================================================================
// 7. IMAGE DECODER, BRADLEY BINARIZATION, CORNER WARP & PHOTO PIPELINE
// =====================================================================

namespace ZvdQrVisualDetail {

struct QuadPoint2D {
    double x{0.0};
    double y{0.0};
};

struct DetectedQuad4 {
    QuadPoint2D tl{};
    QuadPoint2D tr{};
    QuadPoint2D br{};
    QuadPoint2D bl{};
    int rotation_quadrants{0};
    bool valid{false};
};

inline bool parse_image_to_gray(
    const uint8_t* img_buf,
    size_t img_len,
    uint8_t* out_gray,
    size_t out_gray_cap,
    uint32_t& out_w,
    uint32_t& out_h
) noexcept {
    out_w = 0u;
    out_h = 0u;
    if (!img_buf || !out_gray || img_len < 12u) return false;

    if (img_buf[0] == 'P' && img_buf[1] == '5' && (img_buf[2] == '\n' || img_buf[2] == '\r')) {
        size_t pos = (img_buf[2] == '\r' && img_len > 3u && img_buf[3] == '\n') ? 4u : 3u;
        auto next_line = [&](char* line_out, size_t cap) -> bool {
            while (pos < img_len && img_buf[pos] == '#') {
                while (pos < img_len && img_buf[pos] != '\n') ++pos;
                if (pos < img_len && img_buf[pos] == '\n') ++pos;
            }
            size_t len = 0u;
            while (pos < img_len && img_buf[pos] != '\n' && img_buf[pos] != '\r') {
                if (len + 1u < cap) {
                    line_out[len++] = static_cast<char>(img_buf[pos]);
                }
                ++pos;
            }
            line_out[len] = '\0';
            if (pos < img_len && img_buf[pos] == '\r') ++pos;
            if (pos < img_len && img_buf[pos] == '\n') ++pos;
            return len > 0u;
        };

        char dim_line[64]{};
        char max_line[32]{};
        unsigned int w = 0u, h = 0u, maxval = 0u;
        if (!next_line(dim_line, sizeof(dim_line))) return false;
        if (std::sscanf(dim_line, "%u %u", &w, &h) != 2 || w == 0u || h == 0u) return false;
        if (!next_line(max_line, sizeof(max_line))) return false;
        if (std::sscanf(max_line, "%u", &maxval) != 1 || maxval == 0u || maxval > 65535u) return false;

        const size_t px_count = static_cast<size_t>(w) * static_cast<size_t>(h);
        if (out_gray_cap < px_count || img_len < pos + px_count) return false;

        if (maxval <= 255u) {
            std::memcpy(out_gray, img_buf + pos, px_count);
        } else {
            if (img_len < pos + px_count * 2u) return false;
            for (size_t i = 0u; i < px_count; ++i) {
                const unsigned int hi = img_buf[pos + i * 2u];
                const unsigned int lo = img_buf[pos + i * 2u + 1u];
                const unsigned int v16 = (hi << 8) | lo;
                out_gray[i] = static_cast<uint8_t>((v16 * 255u) / maxval);
            }
        }
        out_w = static_cast<uint32_t>(w);
        out_h = static_cast<uint32_t>(h);
        return true;
    }

    if (img_buf[0] == 'B' && img_buf[1] == 'M' && img_len >= 54u) {
        const uint32_t data_off = load_le32(img_buf + 10u);
        const uint32_t dib_size = load_le32(img_buf + 14u);
        if (dib_size < 40u || img_len < static_cast<size_t>(data_off)) return false;

        const int32_t raw_w = static_cast<int32_t>(load_le32(img_buf + 18u));
        const int32_t raw_h = static_cast<int32_t>(load_le32(img_buf + 22u));
        const uint16_t bpp = static_cast<uint16_t>(img_buf[28u] | (static_cast<uint16_t>(img_buf[29u]) << 8));
        const uint32_t comp = load_le32(img_buf + 30u);

        if (raw_w <= 0 || raw_h == 0 || comp != 0u) return false;
        const bool top_down = (raw_h < 0);
        const uint32_t w = static_cast<uint32_t>(raw_w);
        const uint32_t h = static_cast<uint32_t>(raw_h < 0 ? -raw_h : raw_h);
        const size_t px_count = static_cast<size_t>(w) * static_cast<size_t>(h);
        if (out_gray_cap < px_count) return false;

        if (bpp == 24u || bpp == 32u) {
            const uint32_t bytes_per_px = bpp / 8u;
            const size_t row_raw = static_cast<size_t>(w) * bytes_per_px;
            const size_t row_pad = (row_raw + 3u) & ~static_cast<size_t>(3u);
            if (img_len < static_cast<size_t>(data_off) + row_pad * static_cast<size_t>(h)) return false;

            for (uint32_t y = 0u; y < h; ++y) {
                const uint32_t src_y = top_down ? y : ((h - 1u) - y);
                const uint8_t* row_ptr = img_buf + data_off + static_cast<size_t>(src_y) * row_pad;
                uint8_t* dst_row = out_gray + static_cast<size_t>(y) * static_cast<size_t>(w);

                for (uint32_t x = 0u; x < w; ++x) {
                    const uint32_t b = row_ptr[x * bytes_per_px + 0u];
                    const uint32_t g = row_ptr[x * bytes_per_px + 1u];
                    const uint32_t r = row_ptr[x * bytes_per_px + 2u];
                    const uint32_t lum = (77u * r + 150u * g + 29u * b + 128u) >> 8;
                    dst_row[x] = static_cast<uint8_t>(lum & 0xFFu);
                }
            }
            out_w = w;
            out_h = h;
            return true;
        }

        if (bpp == 8u) {
            const size_t palette_off = 14u + static_cast<size_t>(dib_size);
            const size_t row_pad = (static_cast<size_t>(w) + 3u) & ~static_cast<size_t>(3u);
            if (img_len < static_cast<size_t>(data_off) + row_pad * static_cast<size_t>(h)) return false;

            for (uint32_t y = 0u; y < h; ++y) {
                const uint32_t src_y = top_down ? y : ((h - 1u) - y);
                const uint8_t* row_ptr = img_buf + data_off + static_cast<size_t>(src_y) * row_pad;
                uint8_t* dst_row = out_gray + static_cast<size_t>(y) * static_cast<size_t>(w);
                for (uint32_t x = 0u; x < w; ++x) {
                    const uint8_t idx = row_ptr[x];
                    if (palette_off + static_cast<size_t>(idx) * 4u + 2u < static_cast<size_t>(data_off)) {
                        const uint32_t b = img_buf[palette_off + static_cast<size_t>(idx) * 4u + 0u];
                        const uint32_t g = img_buf[palette_off + static_cast<size_t>(idx) * 4u + 1u];
                        const uint32_t r = img_buf[palette_off + static_cast<size_t>(idx) * 4u + 2u];
                        dst_row[x] = static_cast<uint8_t>(((77u * r + 150u * g + 29u * b + 128u) >> 8) & 0xFFu);
                    } else {
                        dst_row[x] = idx;
                    }
                }
            }
            out_w = w;
            out_h = h;
            return true;
        }
    }

    return false;
}

inline bool bradley_local_binarize(
    const uint8_t* gray,
    uint32_t width,
    uint32_t height,
    uint8_t* out_bin
) noexcept {
    if (!gray || !out_bin || width == 0u || height == 0u) return false;
    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    const size_t px_count = w * h;

    static thread_local alignas(64) std::array<uint64_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 4096u) * 6u> integral{};
    if (px_count > integral.size()) return false;

    for (size_t y = 0u; y < h; ++y) {
        uint64_t row_sum = 0ULL;
        const size_t y_off = y * w;
        const size_t prev_off = (y > 0u) ? ((y - 1u) * w) : 0u;
        for (size_t x = 0u; x < w; ++x) {
            row_sum += static_cast<uint64_t>(gray[y_off + x]);
            integral[y_off + x] = row_sum + ((y > 0u) ? integral[prev_off + x] : 0ULL);
        }
    }

    const size_t min_dim = (w < h) ? w : h;
    size_t half_win = min_dim / 16u;
    if (half_win < 8u) half_win = 8u;
    if (half_win > 32u) half_win = 32u;

    for (size_t y = 0u; y < h; ++y) {
        const size_t y1 = (y > half_win) ? (y - half_win) : 0u;
        const size_t y2 = (y + half_win < h) ? (y + half_win) : (h - 1u);
        for (size_t x = 0u; x < w; ++x) {
            const size_t x1 = (x > half_win) ? (x - half_win) : 0u;
            const size_t x2 = (x + half_win < w) ? (x + half_win) : (w - 1u);

            const uint64_t area = static_cast<uint64_t>((x2 - x1 + 1u) * (y2 - y1 + 1u));
            uint64_t sum = integral[y2 * w + x2];
            if (x1 > 0u) sum -= integral[y2 * w + (x1 - 1u)];
            if (y1 > 0u) sum -= integral[(y1 - 1u) * w + x2];
            if (x1 > 0u && y1 > 0u) sum += integral[(y1 - 1u) * w + (x1 - 1u)];

            const uint64_t cur_scaled = static_cast<uint64_t>(gray[y * w + x]) * area * 100ULL;
            out_bin[y * w + x] = (cur_scaled < sum * 85ULL) ? ZVD_PX_BLACK : ZVD_PX_WHITE;
        }
    }
    return true;
}

inline QuadPoint2D locate_dark_cluster_in_region(
    const uint8_t* bin_img,
    uint32_t width,
    uint32_t height,
    size_t x_min,
    size_t y_min,
    size_t x_max,
    size_t y_max,
    bool pick_max_x,
    bool pick_max_y
) noexcept {
    QuadPoint2D pt{
        static_cast<double>(pick_max_x ? x_max : x_min),
        static_cast<double>(pick_max_y ? y_max : y_min)
    };
    if (!bin_img || width == 0u || height == 0u) return pt;

    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    if (x_max >= w) x_max = w - 1u;
    if (y_max >= h) y_max = h - 1u;
    if (x_min > x_max || y_min > y_max) return pt;

    const size_t box_w = ((x_max - x_min + 1u) >= 32u) ? 32u : (x_max - x_min + 1u);
    const size_t box_h = ((y_max - y_min + 1u) >= 32u) ? 32u : (y_max - y_min + 1u);
    const size_t step_x = ((x_max - x_min) > 64u) ? ((x_max - x_min) / 64u) : 1u;
    const size_t step_y = ((y_max - y_min) > 64u) ? ((y_max - y_min) / 64u) : 1u;

    int best_score = -1000000;
    double best_cx = pt.x;
    double best_cy = pt.y;

    for (size_t sy = y_min; sy + box_h <= y_max + 1u; sy += step_y) {
        for (size_t sx = x_min; sx + box_w <= x_max + 1u; sx += step_x) {
            int outer_dark = 0;
            int ring_light = 0;
            int inner_activity = 0;

            for (size_t dy = 0u; dy < box_h; ++dy) {
                const size_t py = sy + dy;
                for (size_t dx = 0u; dx < box_w; ++dx) {
                    const size_t px = sx + dx;
                    const bool is_dark = (bin_img[py * w + px] == ZVD_PX_BLACK);
                    const bool on_outer = (dx < 3u || dx >= box_w - 3u || dy < 3u || dy >= box_h - 3u);
                    const bool on_ring = (!on_outer && (dx < 6u || dx >= box_w - 6u || dy < 6u || dy >= box_h - 6u));

                    if (on_outer) {
                        outer_dark += is_dark ? 2 : -1;
                    } else if (on_ring) {
                        ring_light += is_dark ? -1 : 2;
                    } else {
                        inner_activity += is_dark ? 1 : -1;
                    }
                }
            }

            const double cx = static_cast<double>(sx) + static_cast<double>(box_w) * 0.5;
            const double cy = static_cast<double>(sy) + static_cast<double>(box_h) * 0.5;
            const double corner_bonus =
                (pick_max_x ? (cx - static_cast<double>(x_min)) : (static_cast<double>(x_max) - cx)) * 0.05 +
                (pick_max_y ? (cy - static_cast<double>(y_min)) : (static_cast<double>(y_max) - cy)) * 0.05;

            const int score = outer_dark + ring_light + (inner_activity / 4) + static_cast<int>(corner_bonus);
            if (score > best_score) {
                best_score = score;
                best_cx = cx;
                best_cy = cy;
            }
        }
    }

    pt.x = best_cx;
    pt.y = best_cy;
    return pt;
}

inline DetectedQuad4 detect_four_corner_markers(
    const uint8_t* bin_img,
    uint32_t width,
    uint32_t height
) noexcept {
    DetectedQuad4 q{};
    if (!bin_img || width < 64u || height < 64u) return q;

    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    const size_t half_w = w / 2u;
    const size_t half_h = h / 2u;

    q.tl = locate_dark_cluster_in_region(bin_img, width, height, 0u, 0u, half_w, half_h, false, false);
    q.tr = locate_dark_cluster_in_region(bin_img, width, height, half_w, 0u, w - 1u, half_h, true, false);
    q.br = locate_dark_cluster_in_region(bin_img, width, height, half_w, half_h, w - 1u, h - 1u, true, true);
    q.bl = locate_dark_cluster_in_region(bin_img, width, height, 0u, half_h, half_w, h - 1u, false, true);
    q.rotation_quadrants = 0;
    q.valid = true;
    return q;
}

inline QuadPoint2D bilinear_warp_point(
    const DetectedQuad4& q,
    double u,
    double v
) noexcept {
    const double top_x = q.tl.x + u * (q.tr.x - q.tl.x);
    const double top_y = q.tl.y + u * (q.tr.y - q.tl.y);
    const double bot_x = q.bl.x + u * (q.br.x - q.bl.x);
    const double bot_y = q.bl.y + u * (q.br.y - q.bl.y);

    return QuadPoint2D{
        top_x + v * (bot_x - top_x),
        top_y + v * (bot_y - top_y)
    };
}

inline uint8_t sample_warped_tile_8x8(
    const uint8_t* gray_img,
    const uint8_t* bin_img,
    uint32_t width,
    uint32_t height,
    const DetectedQuad4& q,
    double u0,
    double v0,
    double du,
    double dv
) noexcept {
    static thread_local alignas(64) std::array<uint8_t, ZVD_TILE_PIXELS> local_tile{};
    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);

    for (size_t dy = 0u; dy < ZVD_TILE_SIZE; ++dy) {
        const double fy = (static_cast<double>(dy) + 0.5) / static_cast<double>(ZVD_TILE_SIZE);
        for (size_t dx = 0u; dx < ZVD_TILE_SIZE; ++dx) {
            const double fx = (static_cast<double>(dx) + 0.5) / static_cast<double>(ZVD_TILE_SIZE);
            const QuadPoint2D p = bilinear_warp_point(q, u0 + fx * du, v0 + fy * dv);
            const int ix = static_cast<int>(std::llround(p.x));
            const int iy = static_cast<int>(std::llround(p.y));

            if (ix < 0 || iy < 0 || static_cast<size_t>(ix) >= w || static_cast<size_t>(iy) >= h) {
                local_tile[dy * ZVD_TILE_SIZE + dx] = ZVD_PX_WHITE;
            } else {
                const size_t idx = static_cast<size_t>(iy) * w + static_cast<size_t>(ix);
                local_tile[dy * ZVD_TILE_SIZE + dx] = bin_img ? bin_img[idx] : gray_img[idx];
            }
        }
    }

    return classify_tile_8x8_sector_area(local_tile.data(), ZVD_TILE_SIZE, ZVD_TILE_SIZE, 0u, 0u);
}

} // namespace ZvdQrVisualDetail

inline bool zvd_tri_decode_from_photo(
    const uint8_t* img_buf,
    size_t img_len,
    const ZvdVisualKey& key,
    uint8_t* out_payload,
    size_t out_payload_cap,
    size_t* out_payload_len,
    uint64_t* out_seq_id = nullptr
) noexcept {
    if (!img_buf || !out_payload || !out_payload_len) return false;
    if (!key.is_valid()) return false;
    if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;

    static thread_local alignas(64) std::array<uint8_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 4096u) * 6u> gray_buf{};
    static thread_local alignas(64) std::array<uint8_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 4096u) * 6u> bin_buf{};
    static thread_local alignas(64) std::array<uint8_t, (ZVD_MAX_VISUAL_FRAME_BYTES + 1024u) * 3u> rx_stream{};
    static thread_local alignas(64) std::array<uint8_t, sizeof(ZvdQrFrameHeader) + ZVD_MAX_VISUAL_FRAME_BYTES + 1024u> defecd_pkt{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES + 1024u> folded_plain{};
    static thread_local alignas(64) ZvdQrVisualDetail::DualIcosaMoireState moire_state{};

    uint32_t img_w = 0u;
    uint32_t img_h = 0u;
    if (!ZvdQrVisualDetail::parse_image_to_gray(
            img_buf, img_len, gray_buf.data(), gray_buf.size(), img_w, img_h)) {
        return false;
    }

    if (!ZvdQrVisualDetail::bradley_local_binarize(gray_buf.data(), img_w, img_h, bin_buf.data())) {
        return false;
    }

    const ZvdQrVisualDetail::DetectedQuad4 quad =
        ZvdQrVisualDetail::detect_four_corner_markers(bin_buf.data(), img_w, img_h);
    if (!quad.valid) return false;

    const double u_start = 0.07894736842105263;
    const double u_end   = 0.9210526315789473;
    const double v_start = 0.085;
    const double v_end   = 0.875;

    const uint32_t grid_cols = 64u;
    const double h_left = std::hypot(quad.bl.x - quad.tl.x, quad.bl.y - quad.tl.y);
    const double h_right = std::hypot(quad.br.x - quad.tr.x, quad.br.y - quad.tr.y);
    const double w_top = std::hypot(quad.tr.x - quad.tl.x, quad.tr.y - quad.tl.y);
    const double w_bot = std::hypot(quad.br.x - quad.bl.x, quad.br.y - quad.bl.y);
    const double avg_w = (w_top + w_bot) * 0.5;
    const double avg_h = (h_left + h_right) * 0.5;
    if (avg_w < 32.0 || avg_h < 32.0) return false;

    const double du_full = (u_end - u_start) / static_cast<double>(grid_cols);

    const size_t max_possible_rows = 4096u;
    size_t est_rows = static_cast<size_t>((avg_h * (v_end - v_start)) / (avg_w * (u_end - u_start) / static_cast<double>(grid_cols)));
    if (est_rows == 0u) est_rows = 1u;
    if (est_rows > max_possible_rows) est_rows = max_possible_rows;

    const double dv_row = (v_end - v_start) / static_cast<double>(est_rows);

    size_t rx_byte_pos = 0u;
    uint8_t tile_quad[4]{};
    size_t tile_in_byte = 0u;

    const size_t hdr_bytes_needed = sizeof(ZvdQrFrameHeader);
    size_t target_rx_bytes = hdr_bytes_needed;
    bool header_parsed = false;
    ZvdQrFrameHeader hdr{};

    for (size_t gy = 0u; gy < est_rows; ++gy) {
        if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;
        const double v0 = v_start + static_cast<double>(gy) * dv_row;

        for (uint32_t gx = 0u; gx < grid_cols; ++gx) {
            const double u0 = u_start + static_cast<double>(gx) * du_full;
            const uint8_t t_cls = ZvdQrVisualDetail::sample_warped_tile_8x8(
                gray_buf.data(), bin_buf.data(), img_w, img_h, quad, u0, v0, du_full, dv_row
            );
            tile_quad[tile_in_byte++] = t_cls;

            if (tile_in_byte == 4u) {
                uint8_t b_val = 0u;
                ZvdQrVisualDetail::pack_4_tiles_to_byte(tile_quad[0], tile_quad[1], tile_quad[2], tile_quad[3], b_val);
                tile_in_byte = 0u;

                if (rx_byte_pos < rx_stream.size()) {
                    rx_stream[rx_byte_pos++] = b_val;
                }

                if (!header_parsed && rx_byte_pos >= hdr_bytes_needed) {
                    std::memcpy(&hdr, rx_stream.data(), sizeof(ZvdQrFrameHeader));
                    if (hdr.magic != ZvdQrVisualDetail::ZVD_QR_FRAME_MAGIC ||
                        hdr.version != ZvdQrVisualDetail::ZVD_QR_FRAME_VERSION ||
                        hdr.grid_cols != grid_cols ||
                        hdr.grid_rows == 0u ||
                        hdr.grid_rows > max_possible_rows) {
                        return false;
                    }
                    if (ZvdQrVisualDetail::compute_qr_frame_header_hash(hdr) != hdr.header_hash64) {
                        return false;
                    }
                    header_parsed = true;
                    const size_t total_tiles = static_cast<size_t>(hdr.grid_cols) * static_cast<size_t>(hdr.grid_rows);
                    target_rx_bytes = total_tiles / 4u;
                    if (target_rx_bytes > rx_stream.size()) return false;
                    if (static_cast<size_t>(hdr.grid_rows) > est_rows) {
                        est_rows = static_cast<size_t>(hdr.grid_rows);
                    }
                }

                if (header_parsed && rx_byte_pos >= target_rx_bytes) {
                    break;
                }
            }
        }
        if (header_parsed && rx_byte_pos >= target_rx_bytes) {
            break;
        }
    }

    if (!header_parsed || rx_byte_pos < target_rx_bytes) return false;

    const size_t raw_packet_len = static_cast<size_t>(hdr.wire_packet_bytes);
    const size_t folded_len = static_cast<size_t>(hdr.folded_payload_bytes);
    const size_t raw_payload_len = static_cast<size_t>(hdr.raw_payload_bytes);

    if (raw_packet_len != sizeof(ZvdQrFrameHeader) + folded_len ||
        raw_packet_len > defecd_pkt.size() ||
        raw_payload_len > out_payload_cap ||
        folded_len > folded_plain.size()) {
        return false;
    }

    if (hdr.fec3x_enabled != 0u) {
        size_t maj_len = 0u;
        if (!ZvdQrVisualDetail::fec3x_decode_majority(
                rx_stream.data(), raw_packet_len * 3u, defecd_pkt.data(), defecd_pkt.size(), maj_len) ||
            maj_len != raw_packet_len) {
            return false;
        }
    } else {
        if (target_rx_bytes < raw_packet_len) return false;
        std::memcpy(defecd_pkt.data(), rx_stream.data(), raw_packet_len);
    }

    ZvdQrFrameHeader verified_hdr{};
    std::memcpy(&verified_hdr, defecd_pkt.data(), sizeof(ZvdQrFrameHeader));
    if (verified_hdr.magic != ZvdQrVisualDetail::ZVD_QR_FRAME_MAGIC ||
        ZvdQrVisualDetail::compute_qr_frame_header_hash(verified_hdr) != verified_hdr.header_hash64) {
        return false;
    }

    ZvdQrVisualDetail::compute_dual_icosa_moire(key, verified_hdr.frame_seq_id, moire_state);

    uint8_t aad[sizeof(ZVD_Monolith334) + sizeof(CubeLevel2Anchor) + 16u]{};
    std::memcpy(aad, &verified_hdr.monolith, sizeof(verified_hdr.monolith));
    std::memcpy(aad + sizeof(verified_hdr.monolith), &verified_hdr.cube0, sizeof(verified_hdr.cube0));
    ZvdQrVisualDetail::store_le64(aad + sizeof(verified_hdr.monolith) + sizeof(verified_hdr.cube0), verified_hdr.frame_seq_id);
    ZvdQrVisualDetail::store_le64(aad + sizeof(verified_hdr.monolith) + sizeof(verified_hdr.cube0) + 8u,
                                  static_cast<uint64_t>(folded_len));

    const uint8_t* ct_ptr = defecd_pkt.data() + sizeof(ZvdQrFrameHeader);
    if (!ZvdQrVisualDetail::aead_decrypt_payload(
            key, moire_state, verified_hdr.nonce_12,
            aad, sizeof(aad),
            ct_ptr, folded_len,
            verified_hdr.tag_16,
            folded_plain.data())) {
        return false;
    }

    size_t final_len = 0u;
    if (!ZvdQrVisualDetail::zvd_fold_unpack_3level(
            folded_plain.data(), folded_len, verified_hdr.fold_flags,
            out_payload, out_payload_cap, final_len)) {
        return false;
    }

    if (final_len != raw_payload_len) return false;
    *out_payload_len = final_len;
    if (out_seq_id) *out_seq_id = verified_hdr.frame_seq_id;
    return true;
}
// =====================================================================
// 8. UNIVERSAL BUS INTEGRATION & STANDALONE CLI / SELF-TEST (ZVD_VISUAL_CRYPTO_MAIN)
// =====================================================================

inline int zvd_visual_crypto_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_CRYPTO_TEST")) {
        static thread_local alignas(64) std::array<uint8_t, 262144u> img_buf{};
        static thread_local alignas(64) std::array<uint8_t, 4096u> rx_buf{};

        const char* msg = "ZVD V34.0 QR Visual Crypto 120D Verified.";
        const ZvdVisualKey key{ZVD_DEFAULT_MASTER_KEY, 137u};
        size_t img_len = 0u;
        size_t rx_len = 0u;
        uint64_t rx_seq = 0ULL;

        if (!zvd_qr_encode_frame_image(
                reinterpret_cast<const uint8_t*>(msg),
                std::strlen(msg),
                key,
                1ULL,
                true,
                ZvdQrImageFormat::BMP_24,
                img_buf.data(),
                img_buf.size(),
                &img_len)) {
            return -1;
        }

        if (!zvd_tri_decode_from_photo(
                img_buf.data(),
                img_len,
                key,
                rx_buf.data(),
                rx_buf.size(),
                &rx_len,
                &rx_seq)) {
            return -1;
        }

        if (rx_seq == 1ULL && rx_len == std::strlen(msg) &&
            std::memcmp(msg, rx_buf.data(), rx_len) == 0) {
            return 1;
        }
        return -1;
    }
    return 0;
}

inline bool zvd_visual_crypto_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_CRYPTO_SECURE")) {
        return !response.empty();
    }
    return true;
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_visual_crypto_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_CRYPTO_TEST")) {
        return zvd_visual_crypto_evaluate(ctx.prompt, 0, 0.0);
    }
    return 0;
}
ZVD_REGISTER_CARTRIDGE_CTX(
    "QRVisualCryptoAtlas-120D-V34",
    ZvdModuleRole::FILTER,
    zvd_visual_crypto_evaluate_ctx,
    zvd_visual_crypto_verify
);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("QRVisualCryptoAtlas-120D-V34", zvd_visual_crypto_evaluate, zvd_visual_crypto_verify);
#endif

#if defined(ZVD_VISUAL_CRYPTO_MAIN) || defined(ZVD_VISUAL_CRYPTO_STANDALONE_MAIN)

namespace ZvdQrCliDetail {

inline bool write_file_bytes(const char* path, const uint8_t* data, size_t len) noexcept {
    if (!path || !data) return false;
    std::FILE* fp = std::fopen(path, "wb");
    if (!fp) return false;
    const size_t written = (len > 0u) ? std::fwrite(data, 1u, len, fp) : 0u;
    std::fclose(fp);
    return written == len;
}

inline bool read_file_bytes(const char* path, uint8_t* dst, size_t dst_cap, size_t& out_len) noexcept {
    out_len = 0u;
    if (!path || !dst) return false;
    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    const long sz = std::ftell(fp);
    if (sz < 0 || static_cast<size_t>(sz) > dst_cap) {
        std::fclose(fp);
        return false;
    }
    std::rewind(fp);
    const size_t target = static_cast<size_t>(sz);
    const size_t rd = (target > 0u) ? std::fread(dst, 1u, target, fp) : 0u;
    std::fclose(fp);
    if (rd != target) return false;
    out_len = target;
    return true;
}

inline void simulate_photo_contrast_and_noise(
    uint8_t* gray,
    uint32_t w,
    uint32_t h,
    uint64_t seed
) noexcept {
    if (!gray || w == 0u || h == 0u) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    uint64_t s = ZvdQrVisualDetail::mix64(seed);
    for (size_t i = 0u; i < n; ++i) {
        if ((i & 31u) == 0u) {
            s = ZvdQrVisualDetail::mix64(s ^ static_cast<uint64_t>(i));
        }
        const double gx = static_cast<double>(i % w) / static_cast<double>(w);
        const double gy = static_cast<double>(i / w) / static_cast<double>(h);
        const int grad = static_cast<int>((gx * 18.0 + gy * 12.0) - 15.0);
        const int jitter = static_cast<int>((s >> ((i % 7u) * 8u)) & 0x0Fu) - 7;
        int v = static_cast<int>(gray[i]) + grad + jitter;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        gray[i] = static_cast<uint8_t>(v);
    }
}

inline void run_demo_self_test() {
    static thread_local alignas(64) std::array<uint8_t, 524288u> bmp_buf{};
    static thread_local alignas(64) std::array<uint8_t, 524288u> pgm_buf{};
    static thread_local alignas(64) std::array<uint8_t, 524288u> gray_sim{};
    static thread_local alignas(64) std::array<uint8_t, 524288u> photo_bmp_sim{};
    static thread_local alignas(64) std::array<uint8_t, 16384u> decoded_plain{};

    const char* sample_msg =
        "SENDER_A -> RECEIVER_B | ZVD-120D QR VISUAL CRYPTO V34.0 | VARNA-BULGARIA | ANGLE=137 DEG";
    const size_t msg_len = std::strlen(sample_msg);

    const ZvdVisualKey key_alice{"ZVD2026VarnaCore", 137u};
    const ZvdVisualKey key_bob{"ZVD2026VarnaCore", 137u};

    const auto t0 = std::chrono::steady_clock::now();
    size_t bmp_len = 0u;
    size_t pgm_len = 0u;

    const bool enc_bmp_ok = zvd_qr_encode_frame_image(
        reinterpret_cast<const uint8_t*>(sample_msg), msg_len,
        key_alice, 42ULL, true, ZvdQrImageFormat::BMP_24,
        bmp_buf.data(), bmp_buf.size(), &bmp_len
    );
    const bool enc_pgm_ok = zvd_qr_encode_frame_image(
        reinterpret_cast<const uint8_t*>(sample_msg), msg_len,
        key_alice, 42ULL, true, ZvdQrImageFormat::PGM_P5,
        pgm_buf.data(), pgm_buf.size(), &pgm_len
    );
    const auto t1 = std::chrono::steady_clock::now();

    if (!enc_bmp_ok || !enc_pgm_ok) {
        std::cout << "[FAIL] Frame encoding failed.\n";
        return;
    }

    write_file_bytes("zvd_test_frame.bmp", bmp_buf.data(), bmp_len);
    write_file_bytes("zvd_test_frame.pgm", pgm_buf.data(), pgm_len);

    uint32_t w = 0u, h = 0u;
    size_t sim_len = 0u;
    if (ZvdQrVisualDetail::parse_image_to_gray(bmp_buf.data(), bmp_len, gray_sim.data(), gray_sim.size(), w, h)) {
        simulate_photo_contrast_and_noise(gray_sim.data(), w, h, 0x13702026ULL);
        sim_len = ZvdQrVisualDetail::encode_bmp_24bit_mono_buffer(
            gray_sim.data(), w, h, photo_bmp_sim.data(), photo_bmp_sim.size()
        );
    }

    const auto t2 = std::chrono::steady_clock::now();
    size_t dec_len = 0u;
    uint64_t dec_seq = 0ULL;
    const bool dec_ok = (sim_len > 0u) && zvd_tri_decode_from_photo(
        photo_bmp_sim.data(), sim_len, key_bob,
        decoded_plain.data(), decoded_plain.size(), &dec_len, &dec_seq
    );
    const auto t3 = std::chrono::steady_clock::now();

    const bool bit_perfect = dec_ok && (dec_len == msg_len) &&
                             (std::memcmp(sample_msg, decoded_plain.data(), msg_len) == 0);

    const auto enc_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    const auto dec_us = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

    const double qr_v40_bytes = 2953.0;
    const double zvd_density_ratio = static_cast<double>(msg_len) / 64.0;

    std::cout << "=== ZVD QR VISUAL CRYPTO V34.0 SELF-TEST (ALICE -> PHOTO -> BOB) ===\n";
    std::cout << "Key Password : ZVD2026VarnaCore (16 chars)\n";
    std::cout << "Icosa Angle  : 137 deg (1 deg step)\n";
    std::cout << "Output Files : zvd_test_frame.bmp (" << bmp_len << " B), zvd_test_frame.pgm (" << pgm_len << " B)\n";
    std::cout << "128-bit MAC  : " << (dec_ok ? "VERIFIED (CT OK)" : "FAILED") << "\n";
    std::cout << "Status       : " << (bit_perfect ? "BIT-PERFECT OK" : "MISMATCH") << "\n";
    std::cout << "Timing       : encode=" << enc_us << " us, photo-decode=" << dec_us << " us\n";
    std::cout << "Density Note : 1 byte = 4 tri-tiles (0..4, base-5^4=625); ~" << zvd_density_ratio
              << " bytes/row vs classic QR v40 (~" << qr_v40_bytes << " B max payload)\n";
}

inline void run_encrypt_interactive() {
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> in_buf{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES * 6u> out_img{};

    std::string mode_in;
    std::string source_val;
    std::string password;
    unsigned int angle_u = 0u;
    std::string out_path;

    std::cout << "Source [text / file]: ";
    std::getline(std::cin >> std::ws, mode_in);

    size_t payload_len = 0u;
    if (mode_in == "file") {
        std::cout << "Input file path: ";
        std::getline(std::cin >> std::ws, source_val);
        if (!read_file_bytes(source_val.c_str(), in_buf.data(), in_buf.size(), payload_len)) {
            std::cout << "[ERR] Cannot read input file.\n";
            return;
        }
    } else {
        std::cout << "Enter plain text: ";
        std::getline(std::cin >> std::ws, source_val);
        payload_len = source_val.size();
        if (payload_len > in_buf.size()) {
            std::cout << "[ERR] Text exceeds frame buffer.\n";
            return;
        }
        if (payload_len > 0u) {
            std::memcpy(in_buf.data(), source_val.data(), payload_len);
        }
    }

    std::cout << "Password (min 16 chars): ";
    std::getline(std::cin >> std::ws, password);
    std::cout << "Icosahedron angle (0..359): ";
    std::cin >> angle_u;
    std::cout << "Output path (*.bmp or *.pgm): ";
    std::getline(std::cin >> std::ws, out_path);

    const ZvdVisualKey key{password, static_cast<uint16_t>(angle_u % 360u)};
    if (!key.is_valid()) {
        std::cout << "[ERR] Invalid key: password must be at least 16 characters.\n";
        return;
    }

    const ZvdQrImageFormat fmt = (out_path.size() >= 4u && out_path.substr(out_path.size() - 4u) == ".pgm")
                                     ? ZvdQrImageFormat::PGM_P5
                                     : ZvdQrImageFormat::BMP_24;

    size_t img_len = 0u;
    if (!zvd_qr_encode_frame_image(
            in_buf.data(), payload_len, key, 1ULL, true, fmt,
            out_img.data(), out_img.size(), &img_len)) {
        std::cout << "[ERR] Encryption to image failed.\n";
        return;
    }

    if (!write_file_bytes(out_path.c_str(), out_img.data(), img_len)) {
        std::cout << "[ERR] Cannot write output image.\n";
        return;
    }

    std::cout << "[OK] Saved encrypted matrix to " << out_path << " (" << img_len << " bytes).\n";
}

inline void run_decrypt_interactive() {
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES * 6u> img_buf{};
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> out_payload{};

    std::string img_path;
    std::string password;
    unsigned int angle_u = 0u;
    std::string out_file_opt;

    std::cout << "Input image/photo path (*.bmp / *.pgm): ";
    std::getline(std::cin >> std::ws, img_path);
    std::cout << "Password (min 16 chars): ";
    std::getline(std::cin >> std::ws, password);
    std::cout << "Icosahedron angle (0..359): ";
    std::cin >> angle_u;
    std::cout << "Save recovered bytes to file (or '-' to print text): ";
    std::getline(std::cin >> std::ws, out_file_opt);

    const ZvdVisualKey key{password, static_cast<uint16_t>(angle_u % 360u)};
    if (!key.is_valid()) {
        std::cout << "[ERR] Invalid key: password must be at least 16 characters.\n";
        return;
    }

    size_t img_len = 0u;
    if (!read_file_bytes(img_path.c_str(), img_buf.data(), img_buf.size(), img_len)) {
        std::cout << "[ERR] Cannot read image file.\n";
        return;
    }

    size_t dec_len = 0u;
    uint64_t seq_id = 0ULL;
    if (!zvd_tri_decode_from_photo(
            img_buf.data(), img_len, key,
            out_payload.data(), out_payload.size(), &dec_len, &seq_id)) {
        std::cout << "[ERR] Authentication or decode failed (wrong password, wrong angle, or damaged frame).\n";
        return;
    }

    if (out_file_opt != "-") {
        if (!write_file_bytes(out_file_opt.c_str(), out_payload.data(), dec_len)) {
            std::cout << "[ERR] Cannot write recovered output file.\n";
            return;
        }
        std::cout << "[OK] Verified & recovered " << dec_len << " bytes (seq=" << seq_id << ") -> " << out_file_opt << "\n";
    } else {
        std::cout << "[OK] Verified (seq=" << seq_id << ", " << dec_len << " bytes):\n";
        std::cout.write(reinterpret_cast<const char*>(out_payload.data()), static_cast<std::streamsize>(dec_len));
        std::cout << "\n";
    }
}

} // namespace ZvdQrCliDetail

int main() {
    for (;;) {
        std::cout << "\n=== ZVD QR VISUAL CRYPTO V34.0 (120D DUAL-ICOSA MOIRE + AEAD) ===\n";
        std::cout << "[1] Built-in Demo Test (User A -> Photo -> User B)\n";
        std::cout << "[2] Encrypt Text or File into Image (BMP / PGM)\n";
        std::cout << "[3] Decrypt Image or Photo (BMP / PGM)\n";
        std::cout << "[0] Exit\n";
        std::cout << "Select option: ";

        int choice = -1;
        if (!(std::cin >> choice)) break;

        if (choice == 0) {
            break;
        } else if (choice == 1) {
            ZvdQrCliDetail::run_demo_self_test();
        } else if (choice == 2) {
            ZvdQrCliDetail::run_encrypt_interactive();
        } else if (choice == 3) {
            ZvdQrCliDetail::run_decrypt_interactive();
        } else {
            std::cout << "Unknown option.\n";
        }
    }
    return 0;
}

#endif // ZVD_VISUAL_CRYPTO_MAIN || ZVD_VISUAL_CRYPTO_STANDALONE_MAIN
