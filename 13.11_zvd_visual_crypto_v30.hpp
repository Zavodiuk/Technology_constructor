/* =============================================================================
 *  PROJECT:    ZVD-TETRA-120D BIT-PERFECT VISUAL CRYPTO CODEC & MOIRE ATLAS
 *              (13.11_zvd_visual_crypto_v30.hpp - V33.0-RGB125-DUPLEX)
 *  FILENAME:   13.11_zvd_visual_crypto_v30.hpp
 *  VERSION:    2026.11-V33.0-RGB125-DUPLEX
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution,
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Heap Hot Path, static thread_local alignas(64), Base-125 RGB Duplex
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_VISUAL_CRYPTO 1

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#ifndef ZVD_DEFAULT_MASTER_KEY
#define ZVD_DEFAULT_MASTER_KEY "ZVD-SOVEREIGN-ENTERPRISE-2026"
#endif

#ifndef ZVD_CUBE_RAW_CAPACITY
#define ZVD_CUBE_RAW_CAPACITY 250u
#endif

inline std::atomic<bool> g_zvd_global_shutdown{false};

// =====================================================================
// 1. AUTONOMOUS CORE FALLBACKS & OPTIONAL ZVD MODULE HOOKS
// =====================================================================

namespace ZvdVisualDetail {

inline uint64_t fnv1a_64_seed(const uint8_t* data, size_t len, uint64_t seed = 0xcbf29ce484222325ULL) noexcept {
    uint64_t h = seed;
    if (!data) return h ^ static_cast<uint64_t>(len);
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(data[i]);
        h *= 0x100000001b3ULL;
    }
    h ^= static_cast<uint64_t>(len);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return h;
}

inline uint64_t mix64(uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline int8_t clamp_penta(int v) noexcept {
    if (v < -2) return -2;
    if (v > 2) return 2;
    return static_cast<int8_t>(v);
}

} // namespace ZvdVisualDetail

#if !defined(ZVD_DIM_120)
#define ZVD_DIM_120 120
#endif

#if !defined(ZVD_CELLS_PER_LAYER)
#define ZVD_CELLS_PER_LAYER 500
#endif

#if !defined(ZVD_HAS_DATACENTER_CORE)
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
    int8_t face_data[ZVD_CELLS_PER_LAYER];
    int8_t vertex_data[ZVD_CELLS_PER_LAYER];
} ZVD_TetraCube120D;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif
#endif // !defined(ZVD_HAS_DATACENTER_CORE)

inline constexpr std::array<std::array<int8_t, 3>, 256> ZVD_CRYPTO_UNPACK_BASE5_LUT = []() {
    std::array<std::array<int8_t, 3>, 256> lut{};
    for (int code = 0; code < 256; ++code) {
        int temp = code;
        int a = temp % 5; temp /= 5;
        int b = temp % 5; temp /= 5;
        int c = temp % 5;
        lut[code][0] = static_cast<int8_t>(a - 2);
        lut[code][1] = static_cast<int8_t>(b - 2);
        lut[code][2] = static_cast<int8_t>(c - 2);
    }
    return lut;
}();

inline constexpr uint8_t ZVD_P5_LEVELS[5] = {16u, 72u, 128u, 184u, 240u};

inline uint8_t zvd_decode_p5_channel(uint8_t val) noexcept {
    if (val < 44u) return 0u;
    if (val < 100u) return 1u;
    if (val < 156u) return 2u;
    if (val < 212u) return 3u;
    return 4u;
}

// =====================================================================
// 2. MONOLITH-334 PASSPORT, LGZ-345 STREAM & SESSION KEY DERIVATION
// =====================================================================

inline void zvd_pack_layer_p5_local(const int8_t* layer_500, uint8_t out_167[167]) noexcept {
    int p = 0;
    for (int i = 0; i < 166; ++i) {
        int a = static_cast<int>(ZvdVisualDetail::clamp_penta(layer_500[p++])) + 2;
        int b = static_cast<int>(ZvdVisualDetail::clamp_penta(layer_500[p++])) + 2;
        int c = static_cast<int>(ZvdVisualDetail::clamp_penta(layer_500[p++])) + 2;
        out_167[i] = static_cast<uint8_t>(a + b * 5 + c * 25);
    }
    int d0 = static_cast<int>(ZvdVisualDetail::clamp_penta(layer_500[498])) + 2;
    int d1 = static_cast<int>(ZvdVisualDetail::clamp_penta(layer_500[499])) + 2;
    out_167[166] = static_cast<uint8_t>(d0 + d1 * 5);
}

inline void zvd_visual_ingest_passport(
    const uint8_t* data,
    size_t len,
    ZVD_Monolith334& out_monolith,
    int8_t& out_gl_invariant
) noexcept {
    std::memset(&out_monolith, 0, sizeof(out_monolith));
    out_gl_invariant = 0;
    if (!data || len == 0u) return;

#if defined(ZVD_HAS_DATACENTER_CORE)
    zvd_ensure_luts();
    ZVD_BiCore120D bicore;
    zvd_bicore_init(&bicore, 0, 1);
    ZVD_DatacenterPacket pkt{};
    size_t sample_sz = std::min<size_t>(len, 131072u);
    zvd_node_ingest_monolith_only(data, sample_sz, &bicore, &pkt);
    out_monolith = pkt.monolith;
    out_gl_invariant = pkt.gl_invariant;
#else
    static thread_local alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> f_acc{};
    static thread_local alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> v_acc{};
    f_acc.fill(0);
    v_acc.fill(0);
    size_t stride = (len > 262144u) ? (len / 262144u) : 1u;
    int inv_acc = 0;
    for (size_t i = 0; i < len; i += stride) {
        size_t idx = i % ZVD_CELLS_PER_LAYER;
        int b = static_cast<int>(data[i]);
        f_acc[idx] = ZvdVisualDetail::clamp_penta(static_cast<int>(f_acc[idx]) + ((b % 5) - 2));
        v_acc[idx] = ZvdVisualDetail::clamp_penta(static_cast<int>(v_acc[idx]) + (((b / 5) % 5) - 2));
        inv_acc += static_cast<int>(f_acc[idx]) - static_cast<int>(v_acc[idx]);
    }
    zvd_pack_layer_p5_local(f_acc.data(), out_monolith.face_packed);
    zvd_pack_layer_p5_local(v_acc.data(), out_monolith.vertex_packed);
    out_gl_invariant = ZvdVisualDetail::clamp_penta(inv_acc);
#endif
}

inline void zvd_derive_visual_session_key(
    const char* master_key,
    uint16_t sender_node_id,
    uint16_t receiver_node_id,
    uint64_t frame_seq_id,
    char out_key[256]
) noexcept {
    const char* mk = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
    std::snprintf(
        out_key,
        256,
        "%s|VIS_DUPLEX|SRC%u|DST%u|SEQ%llu",
        mk,
        static_cast<unsigned int>(sender_node_id),
        static_cast<unsigned int>(receiver_node_id),
        static_cast<unsigned long long>(frame_seq_id)
    );
}

inline void zvd_120d_moire_crypt_buffer(
    uint8_t* buf,
    size_t len,
    const char* key_str,
    size_t block_index,
    bool encrypt
) noexcept {
    if (!buf || len == 0u) return;
    const char* k = (key_str && key_str[0] != '\0') ? key_str : ZVD_DEFAULT_MASTER_KEY;
    uint64_t seed = ZvdVisualDetail::fnv1a_64_seed(reinterpret_cast<const uint8_t*>(k), std::strlen(k));
    seed = ZvdVisualDetail::mix64(seed ^ static_cast<uint64_t>(block_index));

    uint64_t state = seed;
    for (size_t i = 0; i < len; ++i) {
        if ((i & 63u) == 0u) {
            state = ZvdVisualDetail::mix64(seed ^ static_cast<uint64_t>(i));
        }
        uint8_t ks = static_cast<uint8_t>(state & 0xFFu);
        state = (state >> 8) | (state << 56);
        if (encrypt) {
            buf[i] = static_cast<uint8_t>((static_cast<uint16_t>(buf[i]) + ks) & 0xFFu);
        } else {
            buf[i] = static_cast<uint8_t>((static_cast<uint16_t>(buf[i]) - ks + 256u) & 0xFFu);
        }
    }
}

inline constexpr uint32_t ZVD_VISUAL_FRAME_MAGIC = 0x4344565Au; // "ZVDC"
inline constexpr uint8_t  ZVD_VISUAL_FLAG_LGZ345 = 0x01u;
inline constexpr size_t   ZVD_MAX_VISUAL_FRAME_BYTES = 16u * 1024u * 1024u;

inline bool zvd_visual_compress_stream(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len,
    uint8_t& out_flags
) noexcept {
    out_flags = 0u;
    out_len = 0u;
    if (!src || !dst || dst_cap < src_len) return false;
    if (src_len == 0u) return true;

#if defined(ZVD_HAS_NESTED_STORAGE_CODEC)
    if (src_len >= 16u && src_len <= 65535u) {
        static thread_local alignas(64) std::array<uint8_t, 65536u> work_buf;
        static thread_local alignas(64) std::array<uint8_t, 131072u> comp_buf;
        std::memcpy(work_buf.data(), src, src_len);
        if (src_len >= 40u) {
            ZvdNestedCodecDetail::tetra_plane_split_forward(work_buf.data(), src_len);
        }
        size_t enc_len = 0u;
        if (ZvdNestedCodecDetail::lgz345_encode_adaptive(
                work_buf.data(), src_len, comp_buf.data(), comp_buf.size(), &enc_len) &&
            enc_len > 0u && enc_len < src_len && enc_len <= dst_cap) {
            std::memcpy(dst, comp_buf.data(), enc_len);
            out_len = enc_len;
            out_flags = ZVD_VISUAL_FLAG_LGZ345;
            return true;
        }
    }
#endif

    std::memcpy(dst, src, src_len);
    out_len = src_len;
    return true;
}

inline bool zvd_visual_decompress_stream(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len,
    uint8_t flags
) noexcept {
    out_len = 0u;
    if (!src || !dst) return false;
    if (src_len == 0u) return true;

#if defined(ZVD_HAS_NESTED_STORAGE_CODEC)
    if ((flags & ZVD_VISUAL_FLAG_LGZ345) != 0u) {
        static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> dec_buf;
        size_t dec_len = 0u;
        if (!ZvdNestedCodecDetail::lgz345_decode_stream(
                src, src_len, dec_buf.data(), dec_buf.size(), &dec_len) ||
            dec_len > dst_cap) {
            return false;
        }
        if (dec_len >= 40u) {
            ZvdNestedCodecDetail::tetra_plane_split_inverse(dec_buf.data(), dec_len);
        }
        std::memcpy(dst, dec_buf.data(), dec_len);
        out_len = dec_len;
        return true;
    }
#else
    (void)flags;
#endif

    if (dst_cap < src_len) return false;
    std::memcpy(dst, src, src_len);
    out_len = src_len;
    return true;
}

// =====================================================================
// 3. DUPLEX VISUAL FRAME HEADER & BASE-125 RGB CODEC
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
ZvdVisualDuplexHeader {
    uint32_t magic;            // "ZVDC" (0x4344565Au)
    uint8_t  flags;            // ZVD_VISUAL_FLAG_LGZ345 if compressed
    int8_t   gl_invariant;     // 120D / GLZ invariant
    uint16_t sender_node_id;   // e.g., 101
    uint16_t receiver_node_id; // e.g., 202
    uint16_t reserved;
    uint32_t width;
    uint32_t height;
    uint64_t frame_seq_id;     // Monotonic session sequence ID
    uint64_t raw_len;          // Original payload length
    uint64_t wire_len;         // Encoded/compressed payload length
    uint64_t payload_hash64;   // FNV-1a / 64-bit hash of original payload
    ZVD_Monolith334 monolith;  // 334-byte visual passport
};
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

inline size_t zvd_encode_base125_rgb_pixels(
    const uint8_t* data,
    size_t len,
    const char* session_key,
    uint8_t* out_rgb_pixels,
    size_t out_rgb_cap
) noexcept {
    if (!out_rgb_pixels) return 0u;
    if (len > 0u && !data) return 0u;

    size_t full_triplets = len / 3u;
    size_t rem_bytes = len % 3u;
    size_t total_pixels = full_triplets * 4u + (rem_bytes > 0u ? 4u : 0u);
    size_t needed_rgb_bytes = total_pixels * 3u;
    if (out_rgb_cap < needed_rgb_bytes) return 0u;

    const char* k = (session_key && session_key[0] != '\0') ? session_key : ZVD_DEFAULT_MASTER_KEY;
    uint64_t k_seed = ZvdVisualDetail::fnv1a_64_seed(reinterpret_cast<const uint8_t*>(k), std::strlen(k));

    size_t dst_idx = 0u;
    size_t pixel_idx = 0u;

    auto emit_quad = [&](uint8_t b0, uint8_t b1, uint8_t b2) noexcept {
        uint8_t digits[12];
        const uint8_t b_arr[3] = {b0, b1, b2};
        for (int t = 0; t < 3; ++t) {
            unsigned int v = static_cast<unsigned int>(b_arr[t]);
            digits[t * 4 + 0] = static_cast<uint8_t>(v % 5u);
            digits[t * 4 + 1] = static_cast<uint8_t>((v / 5u) % 5u);
            digits[t * 4 + 2] = static_cast<uint8_t>((v / 25u) % 5u);
            digits[t * 4 + 3] = static_cast<uint8_t>((v / 125u) % 5u);
        }

        for (int p = 0; p < 4; ++p) {
            uint64_t m = ZvdVisualDetail::mix64(k_seed ^ static_cast<uint64_t>(pixel_idx));
            uint8_t s0 = static_cast<uint8_t>((m >> 0) % 5u);
            uint8_t s1 = static_cast<uint8_t>((m >> 8) % 5u);
            uint8_t s2 = static_cast<uint8_t>((m >> 16) % 5u);

            uint8_t d0 = digits[p * 3 + 0];
            uint8_t d1 = digits[p * 3 + 1];
            uint8_t d2 = digits[p * 3 + 2];

            uint8_t r_m = static_cast<uint8_t>((d0 + s0) % 5u);
            uint8_t g_m = static_cast<uint8_t>((d1 + r_m + s1) % 5u);
            uint8_t b_m = static_cast<uint8_t>((d2 + g_m + s2) % 5u);

            out_rgb_pixels[dst_idx++] = ZVD_P5_LEVELS[r_m];
            out_rgb_pixels[dst_idx++] = ZVD_P5_LEVELS[g_m];
            out_rgb_pixels[dst_idx++] = ZVD_P5_LEVELS[b_m];
            ++pixel_idx;
        }
    };

    for (size_t i = 0; i < full_triplets; ++i) {
        if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return 0u;
        emit_quad(data[i * 3u + 0u], data[i * 3u + 1u], data[i * 3u + 2u]);
    }

    if (rem_bytes > 0u) {
        uint8_t b0 = data[full_triplets * 3u + 0u];
        uint8_t b1 = (rem_bytes > 1u) ? data[full_triplets * 3u + 1u] : 0u;
        emit_quad(b0, b1, 0u);
    }

    return dst_idx;
}

inline bool zvd_decode_base125_rgb_pixels(
    const uint8_t* rgb_pixels,
    size_t rgb_byte_len,
    size_t expected_bytes,
    const char* session_key,
    uint8_t* out_bytes,
    size_t out_cap
) noexcept {
    if (!rgb_pixels || !out_bytes || out_cap < expected_bytes) return false;
    if (expected_bytes == 0u) return true;

    size_t total_pixels = rgb_byte_len / 3u;
    size_t full_quads = total_pixels / 4u;
    const char* k = (session_key && session_key[0] != '\0') ? session_key : ZVD_DEFAULT_MASTER_KEY;
    uint64_t k_seed = ZvdVisualDetail::fnv1a_64_seed(reinterpret_cast<const uint8_t*>(k), std::strlen(k));

    size_t out_pos = 0u;
    for (size_t q = 0; q < full_quads && out_pos < expected_bytes; ++q) {
        if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;
        uint8_t digits[12];
        for (int p = 0; p < 4; ++p) {
            size_t pixel_idx = q * 4u + static_cast<size_t>(p);
            size_t base_px = pixel_idx * 3u;
            uint8_t r_m = zvd_decode_p5_channel(rgb_pixels[base_px + 0u]);
            uint8_t g_m = zvd_decode_p5_channel(rgb_pixels[base_px + 1u]);
            uint8_t b_m = zvd_decode_p5_channel(rgb_pixels[base_px + 2u]);

            uint64_t m = ZvdVisualDetail::mix64(k_seed ^ static_cast<uint64_t>(pixel_idx));
            uint8_t s0 = static_cast<uint8_t>((m >> 0) % 5u);
            uint8_t s1 = static_cast<uint8_t>((m >> 8) % 5u);
            uint8_t s2 = static_cast<uint8_t>((m >> 16) % 5u);

            uint8_t d0 = static_cast<uint8_t>((static_cast<int>(r_m) - static_cast<int>(s0) + 25) % 5);
            uint8_t d1 = static_cast<uint8_t>((static_cast<int>(g_m) - static_cast<int>(r_m) - static_cast<int>(s1) + 25) % 5);
            uint8_t d2 = static_cast<uint8_t>((static_cast<int>(b_m) - static_cast<int>(g_m) - static_cast<int>(s2) + 25) % 5);

            digits[p * 3 + 0] = d0;
            digits[p * 3 + 1] = d1;
            digits[p * 3 + 2] = d2;
        }

        for (int t = 0; t < 3 && out_pos < expected_bytes; ++t) {
            unsigned int v = static_cast<unsigned int>(digits[t * 4 + 0]) +
                             static_cast<unsigned int>(digits[t * 4 + 1]) * 5u +
                             static_cast<unsigned int>(digits[t * 4 + 2]) * 25u +
                             static_cast<unsigned int>(digits[t * 4 + 3]) * 125u;
            out_bytes[out_pos++] = static_cast<uint8_t>(v & 0xFFu);
        }
    }

    return out_pos == expected_bytes;
}

// =====================================================================
// 4. DUPLEX PPM P6 FRAME ENCODE / DECODE & MONOLITH SEARCH
// =====================================================================

namespace ZvdVisualDetail {

inline bool parse_ppm_p6_header(
    const uint8_t* ppm_buf,
    size_t ppm_len,
    bool& has_zvdc_comment,
    unsigned int& out_src,
    unsigned int& out_dst,
    unsigned long long& out_seq,
    uint32_t& out_w,
    uint32_t& out_h,
    size_t& out_data_offset
) noexcept {
    has_zvdc_comment = false;
    out_src = 0u;
    out_dst = 0u;
    out_seq = 0ULL;
    out_w = 0u;
    out_h = 0u;
    out_data_offset = 0u;

    if (!ppm_buf || ppm_len < 12u) return false;
    std::string_view sv(reinterpret_cast<const char*>(ppm_buf), std::min<size_t>(ppm_len, 256u));
    if (!sv.starts_with("P6\n")) return false;

    size_t pos = 3u;
    size_t line_end = sv.find('\n', pos);
    if (line_end == std::string_view::npos) return false;

    std::string line(sv.substr(pos, line_end - pos));
    if (!line.empty() && line[0] == '#') {
        unsigned int c_src = 0u, c_dst = 0u;
        unsigned long long c_seq = 0ULL;
        if (std::sscanf(line.c_str(), "# ZVDC %u %u %llu", &c_src, &c_dst, &c_seq) == 3) {
            has_zvdc_comment = true;
            out_src = c_src;
            out_dst = c_dst;
            out_seq = c_seq;
        }
        pos = line_end + 1u;
        line_end = sv.find('\n', pos);
        if (line_end == std::string_view::npos) return false;
        line.assign(sv.substr(pos, line_end - pos));
    }

    unsigned int w = 0u, h = 0u;
    if (std::sscanf(line.c_str(), "%u %u", &w, &h) != 2 || w == 0u || h == 0u) return false;

    pos = line_end + 1u;
    line_end = sv.find('\n', pos);
    if (line_end == std::string_view::npos) return false;
    std::string max_line(sv.substr(pos, line_end - pos));
    unsigned int maxval = 0u;
    if (std::sscanf(max_line.c_str(), "%u", &maxval) != 1 || maxval != 255u) return false;

    out_w = static_cast<uint32_t>(w);
    out_h = static_cast<uint32_t>(h);
    out_data_offset = line_end + 1u;
    return true;
}

} // namespace ZvdVisualDetail

inline bool zvd_visual_encode_duplex_frame(
    uint16_t sender_node_id,
    uint16_t receiver_node_id,
    uint64_t frame_seq_id,
    const uint8_t* payload,
    size_t payload_len,
    const char* master_key,
    uint8_t* out_ppm_buf,
    size_t out_ppm_cap,
    size_t* out_ppm_len
) noexcept {
    if (!out_ppm_buf || !out_ppm_len) return false;
    if (payload_len > 0u && !payload) return false;
    if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> stage_a;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> stage_b;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> pixels_buf;

    if (payload_len > stage_a.size() || sizeof(ZvdVisualDuplexHeader) + payload_len > stage_b.size()) {
        return false;
    }

    ZVD_Monolith334 mono{};
    int8_t gl_inv = 0;
    zvd_visual_ingest_passport(payload, payload_len, mono, gl_inv);

    size_t comp_len = 0u;
    uint8_t comp_flags = 0u;
    if (!zvd_visual_compress_stream(payload, payload_len, stage_a.data(), stage_a.size(), comp_len, comp_flags)) {
        return false;
    }

    char session_key[256];
    zvd_derive_visual_session_key(master_key, sender_node_id, receiver_node_id, frame_seq_id, session_key);

    if (comp_len > 0u) {
        zvd_120d_moire_crypt_buffer(stage_a.data(), comp_len, session_key, frame_seq_id, true);
    }

    size_t blob_len = sizeof(ZvdVisualDuplexHeader) + comp_len;
    size_t full_triplets = blob_len / 3u;
    size_t rem = blob_len % 3u;
    size_t total_px = full_triplets * 4u + (rem > 0u ? 4u : 0u);
    uint32_t w = 256u;
    uint32_t h = static_cast<uint32_t>((total_px + static_cast<size_t>(w) - 1u) / static_cast<size_t>(w));
    if (h == 0u) h = 1u;

    size_t padded_rgb = static_cast<size_t>(w) * static_cast<size_t>(h) * 3u;
    if (padded_rgb > pixels_buf.size()) return false;

    ZvdVisualDuplexHeader hdr{};
    hdr.magic = ZVD_VISUAL_FRAME_MAGIC;
    hdr.flags = comp_flags;
    hdr.gl_invariant = gl_inv;
    hdr.sender_node_id = sender_node_id;
    hdr.receiver_node_id = receiver_node_id;
    hdr.width = w;
    hdr.height = h;
    hdr.frame_seq_id = frame_seq_id;
    hdr.raw_len = static_cast<uint64_t>(payload_len);
    hdr.wire_len = static_cast<uint64_t>(comp_len);
    hdr.payload_hash64 = ZvdVisualDetail::fnv1a_64_seed(payload, payload_len);
    hdr.monolith = mono;

    if (blob_len > stage_b.size()) return false;
    std::memcpy(stage_b.data(), &hdr, sizeof(ZvdVisualDuplexHeader));
    if (comp_len > 0u) {
        std::memcpy(stage_b.data() + sizeof(ZvdVisualDuplexHeader), stage_a.data(), comp_len);
    }

    size_t encoded_rgb = zvd_encode_base125_rgb_pixels(
        stage_b.data(), blob_len, session_key, pixels_buf.data(), padded_rgb
    );
    if (encoded_rgb == 0u) return false;
    if (padded_rgb > encoded_rgb) {
        std::memset(pixels_buf.data() + encoded_rgb, 0, padded_rgb - encoded_rgb);
    }

    char ppm_hdr[160];
    int hlen = std::snprintf(
        ppm_hdr,
        sizeof(ppm_hdr),
        "P6\n# ZVDC %u %u %llu\n%u %u\n255\n",
        static_cast<unsigned int>(sender_node_id),
        static_cast<unsigned int>(receiver_node_id),
        static_cast<unsigned long long>(frame_seq_id),
        w,
        h
    );
    if (hlen <= 0) return false;

    size_t total_ppm = static_cast<size_t>(hlen) + padded_rgb;
    if (out_ppm_cap < total_ppm) return false;

    std::memcpy(out_ppm_buf, ppm_hdr, static_cast<size_t>(hlen));
    std::memcpy(out_ppm_buf + static_cast<size_t>(hlen), pixels_buf.data(), padded_rgb);
    *out_ppm_len = total_ppm;
    return true;
}

inline bool zvd_visual_extract_header_from_ppm(
    const uint8_t* ppm_buf,
    size_t ppm_len,
    const char* master_key,
    ZvdVisualDuplexHeader& out_hdr
) noexcept {
    bool has_meta = false;
    unsigned int c_src = 0u, c_dst = 0u;
    unsigned long long c_seq = 0ULL;
    uint32_t w = 0u, h = 0u;
    size_t data_off = 0u;

    if (!ZvdVisualDetail::parse_ppm_p6_header(
            ppm_buf, ppm_len, has_meta, c_src, c_dst, c_seq, w, h, data_off) ||
        !has_meta) {
        return false;
    }

    size_t px_bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 3u;
    if (ppm_len < data_off + px_bytes) return false;

    size_t hdr_pixels = ((sizeof(ZvdVisualDuplexHeader) + 2u) / 3u) * 4u;
    if (px_bytes < hdr_pixels * 3u) return false;

    char session_key[256];
    zvd_derive_visual_session_key(
        master_key,
        static_cast<uint16_t>(c_src),
        static_cast<uint16_t>(c_dst),
        static_cast<uint64_t>(c_seq),
        session_key
    );

    static thread_local alignas(64) std::array<uint8_t, sizeof(ZvdVisualDuplexHeader)> hdr_raw;
    if (!zvd_decode_base125_rgb_pixels(
            ppm_buf + data_off,
            hdr_pixels * 3u,
            sizeof(ZvdVisualDuplexHeader),
            session_key,
            hdr_raw.data(),
            hdr_raw.size())) {
        return false;
    }

    std::memcpy(&out_hdr, hdr_raw.data(), sizeof(ZvdVisualDuplexHeader));
    return out_hdr.magic == ZVD_VISUAL_FRAME_MAGIC &&
           out_hdr.sender_node_id == static_cast<uint16_t>(c_src) &&
           out_hdr.receiver_node_id == static_cast<uint16_t>(c_dst) &&
           out_hdr.frame_seq_id == static_cast<uint64_t>(c_seq);
}

inline bool zvd_visual_decode_duplex_frame(
    const uint8_t* ppm_buf,
    size_t ppm_len,
    uint16_t expected_dst_id,
    const char* master_key,
    uint8_t* out_payload,
    size_t out_payload_cap,
    size_t* out_payload_len,
    uint16_t* out_src_id = nullptr,
    uint64_t* out_seq_id = nullptr
) noexcept {
    if (!ppm_buf || !out_payload || !out_payload_len) return false;
    if (g_zvd_global_shutdown.load(std::memory_order_acquire)) return false;

    bool has_meta = false;
    unsigned int c_src = 0u, c_dst = 0u;
    unsigned long long c_seq = 0ULL;
    uint32_t w = 0u, h = 0u;
    size_t data_off = 0u;

    if (!ZvdVisualDetail::parse_ppm_p6_header(
            ppm_buf, ppm_len, has_meta, c_src, c_dst, c_seq, w, h, data_off) ||
        !has_meta) {
        return false;
    }

    if (static_cast<uint16_t>(c_dst) != expected_dst_id) {
        return false;
    }

    size_t px_bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 3u;
    if (ppm_len < data_off + px_bytes || px_bytes > ZVD_MAX_VISUAL_FRAME_BYTES) return false;

    char session_key[256];
    zvd_derive_visual_session_key(
        master_key,
        static_cast<uint16_t>(c_src),
        static_cast<uint16_t>(c_dst),
        static_cast<uint64_t>(c_seq),
        session_key
    );

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> decoded_blob;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_VISUAL_FRAME_BYTES> plain_tmp;

    size_t total_px = px_bytes / 3u;
    size_t full_quads = total_px / 4u;
    size_t max_decoded_bytes = full_quads * 3u;
    if (max_decoded_bytes < sizeof(ZvdVisualDuplexHeader) || max_decoded_bytes > decoded_blob.size()) {
        return false;
    }

    if (!zvd_decode_base125_rgb_pixels(
            ppm_buf + data_off, px_bytes, max_decoded_bytes, session_key, decoded_blob.data(), decoded_blob.size())) {
        return false;
    }

    ZvdVisualDuplexHeader hdr{};
    std::memcpy(&hdr, decoded_blob.data(), sizeof(ZvdVisualDuplexHeader));

    if (hdr.magic != ZVD_VISUAL_FRAME_MAGIC || hdr.receiver_node_id != expected_dst_id) {
        return false;
    }

    size_t wire_len = static_cast<size_t>(hdr.wire_len);
    size_t raw_len = static_cast<size_t>(hdr.raw_len);
    if (sizeof(ZvdVisualDuplexHeader) + wire_len > max_decoded_bytes ||
        raw_len > out_payload_cap || wire_len > plain_tmp.size()) {
        return false;
    }

    if (wire_len > 0u) {
        std::memcpy(plain_tmp.data(), decoded_blob.data() + sizeof(ZvdVisualDuplexHeader), wire_len);
        zvd_120d_moire_crypt_buffer(plain_tmp.data(), wire_len, session_key, hdr.frame_seq_id, false);
    }

    size_t final_len = 0u;
    if (!zvd_visual_decompress_stream(plain_tmp.data(), wire_len, out_payload, out_payload_cap, final_len, hdr.flags)) {
        return false;
    }

    if (final_len != raw_len) return false;
    if (ZvdVisualDetail::fnv1a_64_seed(out_payload, final_len) != hdr.payload_hash64) {
        return false;
    }

    *out_payload_len = final_len;
    if (out_src_id) *out_src_id = hdr.sender_node_id;
    if (out_seq_id) *out_seq_id = hdr.frame_seq_id;
    return true;
}

inline bool zvd_visual_search_by_monolith(
    const uint8_t* ppm_buf,
    size_t ppm_len,
    const ZVD_Monolith334& target_monolith,
    const char* master_key,
    double* out_similarity
) noexcept {
    ZvdVisualDuplexHeader hdr{};
    if (!zvd_visual_extract_header_from_ppm(ppm_buf, ppm_len, master_key, hdr)) {
        return false;
    }

#if defined(ZVD_HAS_NESTED_STORAGE_CODEC)
    double score = zvd_nested_resonance_score(&hdr.monolith, &target_monolith);
    if (out_similarity) *out_similarity = score;
    return score >= 0.5;
#else
    size_t matching_cells = 0u;
    size_t total_cells = 0u;
    for (size_t i = 0; i < 167u; ++i) {
        const auto& f_a = ZVD_CRYPTO_UNPACK_BASE5_LUT[hdr.monolith.face_packed[i]];
        const auto& f_b = ZVD_CRYPTO_UNPACK_BASE5_LUT[target_monolith.face_packed[i]];
        const auto& v_a = ZVD_CRYPTO_UNPACK_BASE5_LUT[hdr.monolith.vertex_packed[i]];
        const auto& v_b = ZVD_CRYPTO_UNPACK_BASE5_LUT[target_monolith.vertex_packed[i]];

        int count = (i < 166u) ? 3 : 2;
        for (int k = 0; k < count; ++k) {
            if (std::abs(static_cast<int>(f_a[k]) - static_cast<int>(f_b[k])) <= 1) ++matching_cells;
            if (std::abs(static_cast<int>(v_a[k]) - static_cast<int>(v_b[k])) <= 1) ++matching_cells;
            total_cells += 2u;
        }
    }
    double score = (total_cells > 0u)
                       ? (static_cast<double>(matching_cells) / static_cast<double>(total_cells))
                       : 0.0;
    if (out_similarity) *out_similarity = score;
    return score >= 0.5;
#endif
}
// =====================================================================
// 5. UNIVERSAL BUS INTEGRATION
// =====================================================================

inline int zvd_visual_crypto_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_CRYPTO_TEST")) {
        static thread_local alignas(64) std::array<uint8_t, 65536> ppm_out;
        static thread_local alignas(64) std::array<uint8_t, 1024> rx_out;
        const char* msg = "ZVD V33.0 Duplex Visual Crypto Verified.";
        size_t rx_len = 0u;
        size_t ppm_len = 0u;
        uint16_t src = 0u;
        uint64_t seq = 0u;

        if (!zvd_visual_encode_duplex_frame(
                101u, 202u, 1ULL,
                reinterpret_cast<const uint8_t*>(msg), std::strlen(msg),
                ZVD_DEFAULT_MASTER_KEY, ppm_out.data(), ppm_out.size(), &ppm_len)) {
            return -1;
        }
        if (!zvd_visual_decode_duplex_frame(
                ppm_out.data(), ppm_len, 202u, ZVD_DEFAULT_MASTER_KEY,
                rx_out.data(), rx_out.size(), &rx_len, &src, &seq)) {
            return -1;
        }
        if (src == 101u && seq == 1ULL && rx_len == std::strlen(msg) &&
            std::memcmp(msg, rx_out.data(), rx_len) == 0) {
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
    "VisualCryptoAtlas-120D-V33",
    ZvdModuleRole::FILTER,
    zvd_visual_crypto_evaluate_ctx,
    zvd_visual_crypto_verify
);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("VisualCryptoAtlas-120D-V33", zvd_visual_crypto_evaluate, zvd_visual_crypto_verify);
#endif
