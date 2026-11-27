/* =============================================================================
 *  PROJECT:    ZVD-120D NESTED GEOMETRIC CODEC & MULTI-LEVEL STORAGE CARTRIDGE
 *              (13.11_zvd_nested_storage_codec_v32.hpp - V32.1-SUPREME-C20)
 *  FILENAME:   13.11_zvd_nested_storage_codec_v32.hpp
 *  VERSION:    2026.11-V32.1-SUPREME-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution,
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Heap Hot Path, alignas(64), Atomic .zvd3 Vault, OpenMP Safe
 * =============================================================================
 */

#pragma once

// =====================================================================
// SECTION 1: CORE INCLUSION GUARD & STANDARD HEADERS
// =====================================================================

#define ZVD_HAS_NESTED_STORAGE_CODEC 1

#ifndef ZVD_HAS_DATACENTER_CORE
  #if defined(__has_include)
    #if __has_include("13.11_zvd_datacenter_node_v28_lite.c")
      extern "C" {
      #include "13.11_zvd_datacenter_node_v28_lite.c"
      }
      #define ZVD_HAS_DATACENTER_CORE 1
    #else
      #error "[ZVD-CONSTRUCTOR ERROR] Missing 13.11_zvd_datacenter_node_v28_lite.c!"
    #endif
  #else
    extern "C" {
    #include "13.11_zvd_datacenter_node_v28_lite.c"
    }
    #define ZVD_HAS_DATACENTER_CORE 1
  #endif
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef _WIN32
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// =====================================================================
// SECTION 2: TOPOLOGY MODES, CONSTANTS & RUNTIME SWITCHER
// =====================================================================

enum class ZvdCodecTopologyMode : uint8_t {
    FLAT_ZVDO = 0,
    STELLA_8_VERT = 1,
    EDGE12_TETRA = 2
};

inline constexpr uint32_t ZVD3_MAGIC = 0x3344565Au; // "ZVD3" little-endian
inline constexpr uint32_t ZVD3_VERSION = 4u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_COMPRESSED = 0x80000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_EGYPT345 = 0x40000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_PLANE_SPLIT = 0x20000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_LGZ345 = 0x10000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_POST_PYTH = 0x08000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAGS_MASK =
    ZVD3_SLOT_FLAG_COMPRESSED |
    ZVD3_SLOT_FLAG_EGYPT345 |
    ZVD3_SLOT_FLAG_PLANE_SPLIT |
    ZVD3_SLOT_FLAG_LGZ345 |
    ZVD3_SLOT_FLAG_POST_PYTH;
inline constexpr uint32_t ZVD3_SLOT_SIZE_MASK = ~ZVD3_SLOT_FLAGS_MASK;
inline constexpr uint16_t ZVD3_MAX_BASE5_CODE = 624u; // 4 * (0 + 5*4 + 25*4 + 125*4) = 624

inline constexpr uint16_t ZVD_MICRO_SLOTS_COUNT = 10u; // 0..3 vertices (4), 4..9 edges (6)
inline constexpr uint16_t ZVD_VERTEX_SLOTS_COUNT = 4u;
inline constexpr uint16_t ZVD_EDGE_SLOTS_COUNT = 6u;

inline constexpr size_t ZVD_STELLA8_L1_TETRAS = 8u;
inline constexpr size_t ZVD_STELLA8_CUBES_PER_TETRA = 6u;
inline constexpr size_t ZVD_STELLA8_L2_CUBES = ZVD_STELLA8_L1_TETRAS * ZVD_STELLA8_CUBES_PER_TETRA; // 48
inline constexpr size_t ZVD_STELLA8_MICROS_PER_CUBE = 12u;
inline constexpr size_t ZVD_STELLA8_L3_MICROS = ZVD_STELLA8_L2_CUBES * ZVD_STELLA8_MICROS_PER_CUBE; // 576
inline constexpr size_t ZVD_STELLA8_SLOT_BYTES = 704u; // 5760 slots * 704 B = 4,055,040 B (~3.87 MB)

inline constexpr size_t ZVD_EDGE12_L1_TETRAS = 12u;
inline constexpr size_t ZVD_EDGE12_CUBES_PER_TETRA = 6u;
inline constexpr size_t ZVD_EDGE12_L2_CUBES = ZVD_EDGE12_L1_TETRAS * ZVD_EDGE12_CUBES_PER_TETRA; // 72
inline constexpr size_t ZVD_EDGE12_MICROS_PER_CUBE = 12u;
inline constexpr size_t ZVD_EDGE12_L3_MICROS = ZVD_EDGE12_L2_CUBES * ZVD_EDGE12_MICROS_PER_CUBE; // 864
inline constexpr size_t ZVD_EDGE12_SLOT_BYTES = 448u; // 8640 slots * 448 B = 3,870,720 B (~3.69 MB)

inline constexpr size_t ZVD_ICOSA_ANCHOR_BYTES = 120u; // 60D face + 60D vertex global profile
inline constexpr size_t ZVD_CUBE_ANCHOR_BYTES = 14u;   // 8 vertices + 6 faces per cube
inline constexpr size_t ZVD_PYTH_LCM_PATTERN_BYTES = 840u; // LCM(14, 120) = 840

inline constexpr size_t ZVD_MAX_MACRO_CELL_BYTES = ZVD_STELLA8_L3_MICROS * ZVD_MICRO_SLOTS_COUNT * ZVD_STELLA8_SLOT_BYTES;
inline constexpr size_t ZVD_MAX_MICRO_RAW_BYTES = ZVD_MICRO_SLOTS_COUNT * ZVD_STELLA8_SLOT_BYTES;
inline constexpr size_t ZVD_MICRO_LEN_TABLE_BYTES = ZVD_MICRO_SLOTS_COUNT * sizeof(uint16_t);

namespace ZvdNestedCodecDetail {

inline std::atomic<uint8_t>& active_mode_atomic() noexcept {
    static std::atomic<uint8_t> mode{static_cast<uint8_t>(ZvdCodecTopologyMode::STELLA_8_VERT)};
    return mode;
}

inline ZvdCodecTopologyMode parse_mode_str(const char* s) noexcept {
    if (!s || s[0] == '\0') {
        return ZvdCodecTopologyMode::STELLA_8_VERT;
    }
    if (std::strcmp(s, "flat") == 0 || std::strcmp(s, "FLAT_ZVDO") == 0 || std::strcmp(s, "0") == 0) {
        return ZvdCodecTopologyMode::FLAT_ZVDO;
    }
    if (std::strcmp(s, "edge12") == 0 || std::strcmp(s, "EDGE12_TETRA") == 0 || std::strcmp(s, "2") == 0) {
        return ZvdCodecTopologyMode::EDGE12_TETRA;
    }
    return ZvdCodecTopologyMode::STELLA_8_VERT;
}

inline void init_mode_from_env_once() noexcept {
    static std::atomic<bool> initialized{false};
    bool expected = false;
    if (initialized.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        const char* env_val = std::getenv("ZVD_CODEC_MODE");
        active_mode_atomic().store(static_cast<uint8_t>(parse_mode_str(env_val)), std::memory_order_release);
    }
}

} // namespace ZvdNestedCodecDetail

inline void zvd_set_codec_mode(ZvdCodecTopologyMode mode) noexcept {
    ZvdNestedCodecDetail::active_mode_atomic().store(static_cast<uint8_t>(mode), std::memory_order_release);
}

inline ZvdCodecTopologyMode zvd_get_codec_mode() noexcept {
    ZvdNestedCodecDetail::init_mode_from_env_once();
    return static_cast<ZvdCodecTopologyMode>(ZvdNestedCodecDetail::active_mode_atomic().load(std::memory_order_acquire));
}

inline const char* zvd_codec_mode_name(ZvdCodecTopologyMode mode) noexcept {
    switch (mode) {
        case ZvdCodecTopologyMode::FLAT_ZVDO: return "FLAT_ZVDO";
        case ZvdCodecTopologyMode::STELLA_8_VERT: return "STELLA_8_VERT";
        case ZvdCodecTopologyMode::EDGE12_TETRA: return "EDGE12_TETRA";
        default: return "STELLA_8_VERT";
    }
}

// =====================================================================
// SECTION 3: PACKED DISK HEADER, LEVEL-2 CUBE, 18-BYTE DIR & WORKSPACE
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
CubeLevel2Anchor {
    uint8_t data[ZVD_CUBE_ANCHOR_BYTES]; // 8 vertices + 6 faces relative to Level-1 Icosahedron
};

struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
MacroCellDiskHeader {
    uint32_t magic;            // "ZVD3" (0x3344565Au)
    uint32_t version;          // 4
    uint8_t topology_mode;     // ZvdCodecTopologyMode
    uint8_t reserved_pad[7];
    uint64_t raw_payload_bytes;
    uint64_t packed_payload_bytes;
    uint64_t root_hash64;
    uint64_t header_hash64;    // Integrity hash over header fields prior to header_hash64
    uint64_t cubes_hash64;     // Integrity hash over Level-2 CubeLevel2Anchor table
    uint64_t directory_hash64; // Integrity hash over Level-3 MicroTetraDirectoryEntry table
    int8_t root_gl_invariant;
    uint8_t reserved_inv[7];
    ZVD_Monolith334 root_monolith; // 334 bytes
    uint8_t icosa_anchor_120[ZVD_ICOSA_ANCHOR_BYTES]; // Level-1 60D face + 60D vertex global profile
    uint32_t active_cube_count;
    uint32_t cubes_offset;
    uint32_t active_micro_count;
    uint32_t directory_offset;
    uint32_t payload_base_offset;
    uint32_t reserved_tail;
};

struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
MicroTetraDirectoryEntry {
    uint16_t geo_id;               // Micro-tetra geometric index
    uint16_t vertex_base5_code;    // Base-5 packed 4 vertices (must be <= 624u)
    uint16_t slot_mask;            // Bit k set => active slot k in 0..9
    uint16_t raw_micro_bytes;      // Uncompressed byte count across active slots (<= 7040)
    uint32_t stored_len_and_flags; // ZVD3_SLOT_FLAG_* | wire_len
    uint32_t coupled_micro_tag32;  // Coupled vertex-edge-cube-micro integrity tag
    uint32_t file_offset;          // Byte offset in .zvd3 file to packed micro-tetra blob
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

static_assert(sizeof(CubeLevel2Anchor) == 14u, "CubeLevel2Anchor must be exactly 14 bytes");
static_assert(sizeof(MicroTetraDirectoryEntry) == 18u, "MicroTetraDirectoryEntry must be exactly 18 bytes");

struct alignas(64) MacroCellWorkspace {
    ZvdCodecTopologyMode mode = ZvdCodecTopologyMode::STELLA_8_VERT;
    size_t l1_tetras = 8;
    size_t l2_cubes = 48;
    size_t l3_micros = 576;
    size_t slot_bytes = 704;
    size_t active_cube_count = 0;
    size_t active_micro_count = 0;
    size_t raw_payload_bytes = 0;
    uint64_t root_hash64 = 0;
    int8_t root_gl_invariant = 0;
    ZVD_Monolith334 root_monolith{};
    std::array<uint8_t, ZVD_ICOSA_ANCHOR_BYTES> icosa_anchor_120{};
    std::array<CubeLevel2Anchor, ZVD_EDGE12_L2_CUBES> cubes{};
    std::array<MicroTetraDirectoryEntry, ZVD_EDGE12_L3_MICROS> directory{};
    std::array<uint8_t, ZVD_MAX_MACRO_CELL_BYTES + (ZVD_EDGE12_L3_MICROS * ZVD_MICRO_LEN_TABLE_BYTES)> payload_arena{};
    size_t payload_used = 0;
};

// =====================================================================
// SECTION 4: PYTHAGOREAN 3-4-5 LUT, PLANE-SPLIT & LGZ-345 CASCADE
// =====================================================================

namespace ZvdNestedCodecDetail {

inline constexpr std::array<std::array<uint8_t, 2>, ZVD_EDGE_SLOTS_COUNT> TETRA_EDGE_PAIRS = {{
    {{0u, 1u}}, // edge slot 4
    {{0u, 2u}}, // edge slot 5
    {{0u, 3u}}, // edge slot 6
    {{1u, 2u}}, // edge slot 7
    {{1u, 3u}}, // edge slot 8
    {{2u, 3u}}  // edge slot 9
}};

inline std::array<std::array<uint8_t, 256>, 256>& pyth_345_lut_storage() noexcept {
    static alignas(64) std::array<std::array<uint8_t, 256>, 256> lut{};
    return lut;
}

inline void ensure_pyth_345_lut() noexcept {
    static std::atomic<bool> ready{false};
    if (ready.load(std::memory_order_acquire)) return;

    static std::atomic<bool> building{false};
    bool expected = false;
    if (building.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        auto& lut = pyth_345_lut_storage();
        for (size_t a = 0; a < 256u; ++a) {
            double da = static_cast<double>(a);
            double term_a = 9.0 * da * da;
            for (size_t b = 0; b < 256u; ++b) {
                double db = static_cast<double>(b);
                double hyp = std::sqrt(term_a + (16.0 * db * db));
                uint32_t h_floor = static_cast<uint32_t>(std::floor(hyp));
                lut[a][b] = static_cast<uint8_t>(h_floor & 0xFFu);
            }
        }
        ready.store(true, std::memory_order_release);
    } else {
        while (!ready.load(std::memory_order_acquire)) {
#if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
#endif
        }
    }
}

inline uint8_t pyth_345_predict_byte(
    uint8_t cube_byte,
    uint8_t icosa_byte
) noexcept {
    ensure_pyth_345_lut();
    return pyth_345_lut_storage()[cube_byte][icosa_byte];
}

inline void pyth_345_transform_buffer(
    uint8_t* buf,
    size_t len,
    const uint8_t icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    const CubeLevel2Anchor& cube_anchor,
    bool forward
) noexcept {
    if (!buf || len == 0 || !icosa_120) return;
    ensure_pyth_345_lut();
    const auto& lut = pyth_345_lut_storage();

    static thread_local alignas(64) std::array<uint8_t, ZVD_PYTH_LCM_PATTERN_BYTES> pattern{};
    size_t pat_len = (len < ZVD_PYTH_LCM_PATTERN_BYTES) ? len : ZVD_PYTH_LCM_PATTERN_BYTES;

    size_t idx14 = 0u;
    size_t idx120 = 0u;
    for (size_t k = 0; k < pat_len; ++k) {
        uint8_t a = cube_anchor.data[idx14];
        uint8_t b = icosa_120[idx120];
        pattern[k] = lut[a][b];
        if (++idx14 == ZVD_CUBE_ANCHOR_BYTES) idx14 = 0u;
        if (++idx120 == ZVD_ICOSA_ANCHOR_BYTES) idx120 = 0u;
    }

    size_t pos = 0u;
    if (forward) {
        while (pos < len) {
            size_t chunk = len - pos;
            if (chunk > pat_len) chunk = pat_len;
            uint8_t* dst = buf + pos;
            for (size_t k = 0; k < chunk; ++k) {
                dst[k] = static_cast<uint8_t>((static_cast<unsigned>(dst[k]) - static_cast<unsigned>(pattern[k])) & 0xFFu);
            }
            pos += chunk;
        }
    } else {
        while (pos < len) {
            size_t chunk = len - pos;
            if (chunk > pat_len) chunk = pat_len;
            uint8_t* dst = buf + pos;
            for (size_t k = 0; k < chunk; ++k) {
                dst[k] = static_cast<uint8_t>((static_cast<unsigned>(dst[k]) + static_cast<unsigned>(pattern[k])) & 0xFFu);
            }
            pos += chunk;
        }
    }
}

inline void geometry_dims_for_mode(
    ZvdCodecTopologyMode mode,
    size_t& out_l1,
    size_t& out_l2,
    size_t& out_l3,
    size_t& out_slot_bytes
) noexcept {
    if (mode == ZvdCodecTopologyMode::EDGE12_TETRA) {
        out_l1 = ZVD_EDGE12_L1_TETRAS;
        out_l2 = ZVD_EDGE12_L2_CUBES;
        out_l3 = ZVD_EDGE12_L3_MICROS;
        out_slot_bytes = ZVD_EDGE12_SLOT_BYTES;
    } else {
        out_l1 = ZVD_STELLA8_L1_TETRAS;
        out_l2 = ZVD_STELLA8_L2_CUBES;
        out_l3 = ZVD_STELLA8_L3_MICROS;
        out_slot_bytes = ZVD_STELLA8_SLOT_BYTES;
    }
}

inline uint16_t pack_vertices_base5(int8_t v0, int8_t v1, int8_t v2, int8_t v3) noexcept {
    uint16_t u0 = static_cast<uint16_t>(zvd_lukasiewicz_clamp(v0) + 2);
    uint16_t u1 = static_cast<uint16_t>(zvd_lukasiewicz_clamp(v1) + 2);
    uint16_t u2 = static_cast<uint16_t>(zvd_lukasiewicz_clamp(v2) + 2);
    uint16_t u3 = static_cast<uint16_t>(zvd_lukasiewicz_clamp(v3) + 2);
    return static_cast<uint16_t>(u0 + 5u * u1 + 25u * u2 + 125u * u3);
}

inline void unpack_vertices_base5(uint16_t code, int8_t out_v[4]) noexcept {
    uint16_t rem = code;
    out_v[0] = static_cast<int8_t>((rem % 5u) - 2);
    rem /= 5u;
    out_v[1] = static_cast<int8_t>((rem % 5u) - 2);
    rem /= 5u;
    out_v[2] = static_cast<int8_t>((rem % 5u) - 2);
    rem /= 5u;
    out_v[3] = static_cast<int8_t>((rem % 5u) - 2);
}

inline int8_t derive_vertex_p5_from_slot(const uint8_t* data, size_t len, uint16_t geo_id, uint16_t v_slot) noexcept {
    if (!data || len == 0) {
        return static_cast<int8_t>((static_cast<int>(geo_id + v_slot) % 5) - 2);
    }
    uint64_t h = zvd_fast_hash64(data, len);
    uint64_t mixed = h ^ (static_cast<uint64_t>(geo_id) * 0x9E3779B97F4A7C15ULL) ^ (static_cast<uint64_t>(v_slot) * 0xBF58476D1CE4E5B9ULL);
    int raw5 = static_cast<int>((mixed ^ (mixed >> 29)) % 5ULL);
    return static_cast<int8_t>(raw5 - 2);
}

inline void tetra_plane_split_forward(uint8_t* buf, size_t len) noexcept {
    if (!buf || len < 4u) return;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 2048> tmp{};
    if (len > tmp.size()) return;

    size_t q_len = len / 4u;
    size_t rem = len % 4u;
    size_t p0 = 0u;
    size_t p1 = q_len + ((rem > 0u) ? 1u : 0u);
    size_t p2 = p1 + q_len + ((rem > 1u) ? 1u : 0u);
    size_t p3 = p2 + q_len + ((rem > 2u) ? 1u : 0u);

    for (size_t i = 0; i < len; ++i) {
        switch (i & 3u) {
            case 0u: tmp[p0++] = buf[i]; break;
            case 1u: tmp[p1++] = buf[i]; break;
            case 2u: tmp[p2++] = buf[i]; break;
            default: tmp[p3++] = buf[i]; break;
        }
    }
    std::memcpy(buf, tmp.data(), len);
}

inline void tetra_plane_split_inverse(uint8_t* buf, size_t len) noexcept {
    if (!buf || len < 4u) return;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 2048> tmp{};
    if (len > tmp.size()) return;

    size_t q_len = len / 4u;
    size_t rem = len % 4u;
    size_t p0 = 0u;
    size_t p1 = q_len + ((rem > 0u) ? 1u : 0u);
    size_t p2 = p1 + q_len + ((rem > 1u) ? 1u : 0u);
    size_t p3 = p2 + q_len + ((rem > 2u) ? 1u : 0u);

    for (size_t i = 0; i < len; ++i) {
        switch (i & 3u) {
            case 0u: tmp[i] = buf[p0++]; break;
            case 1u: tmp[i] = buf[p1++]; break;
            case 2u: tmp[i] = buf[p2++]; break;
            default: tmp[i] = buf[p3++]; break;
        }
    }
    std::memcpy(buf, tmp.data(), len);
}

// LGZ-345 (Lukasiewicz-Godel-Zavodiuk 3 -> 4 -> 5) cascade:
// Phase 1 (Trit / Zero-bitmap): zero bytes are omitted from payload.
// Phase 2 (Quadrant): 2-bit class per non-zero byte:
//   0 = small pentary {-2,-1,+1,+2} (packed 3-per-byte in base-5: u0 + 5*u1 + 25*u2 < 125)
//   1 = medium delta [-15..+15] (excluding {-2,-1,0,+1,+2})
//   2 = full signed byte [-128..+127]
//   3 = reserved
inline bool lgz345_encode_adaptive(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len
) noexcept {
    if (!src || !dst || src_len == 0 || src_len > 65535u || dst_cap < 16u) return false;

    const size_t n = src_len;
    const size_t zero_mask_bytes = (n + 7u) / 8u;
    const size_t quad_bytes = (n + 3u) / 4u;

    static thread_local alignas(64) std::array<uint8_t, 8192> zero_mask{};
    static thread_local alignas(64) std::array<uint8_t, 16384> quad_stream{};
    static thread_local alignas(64) std::array<uint8_t, 65536> pent_small_buf{};
    static thread_local alignas(64) std::array<uint8_t, 65536> med_buf{};
    static thread_local alignas(64) std::array<uint8_t, 65536> full_buf{};
    static thread_local alignas(64) std::array<uint8_t, 65536> small_u_vals{};

    if (zero_mask_bytes > zero_mask.size() || quad_bytes > quad_stream.size()) return false;

    std::memset(zero_mask.data(), 0, zero_mask_bytes);
    std::memset(quad_stream.data(), 0, quad_bytes);

    size_t small_u_count = 0;
    size_t med_count = 0;
    size_t full_count = 0;

    for (size_t i = 0; i < n; ++i) {
        int8_t sv = static_cast<int8_t>(src[i]);
        int iv = static_cast<int>(sv);
        if (iv == 0) {
            continue;
        }
        zero_mask[i >> 3] = static_cast<uint8_t>(zero_mask[i >> 3] | static_cast<uint8_t>(1u << (i & 7u)));

        uint8_t q_code = 0u;
        if (iv >= -2 && iv <= 2) {
            // Non-zero small pentary: {-2 -> 0, -1 -> 1, +1 -> 3, +2 -> 4}
            q_code = 0u;
            small_u_vals[small_u_count++] = static_cast<uint8_t>(iv + 2);
        } else if (iv >= -15 && iv <= 15) {
            q_code = 1u;
            med_buf[med_count++] = static_cast<uint8_t>(iv & 0xFF);
        } else {
            q_code = 2u;
            full_buf[full_count++] = static_cast<uint8_t>(iv & 0xFF);
        }

        size_t q_byte_idx = i >> 2;
        unsigned q_shift = static_cast<unsigned>((i & 3u) * 2u);
        quad_stream[q_byte_idx] = static_cast<uint8_t>(
            quad_stream[q_byte_idx] | static_cast<uint8_t>((q_code & 0x3u) << q_shift)
        );
    }

    size_t pent_wire_count = 0;
    for (size_t k = 0; k < small_u_count; k += 3u) {
        unsigned u0 = static_cast<unsigned>(small_u_vals[k]);
        unsigned u1 = (k + 1u < small_u_count) ? static_cast<unsigned>(small_u_vals[k + 1u]) : 0u;
        unsigned u2 = (k + 2u < small_u_count) ? static_cast<unsigned>(small_u_vals[k + 2u]) : 0u;
        pent_small_buf[pent_wire_count++] = static_cast<uint8_t>((u0 + 5u * u1 + 25u * u2) & 0xFFu);
    }

    // Header layout (12 bytes):
    // [0..1] uint16_t orig_len
    // [2..3] uint16_t small_u_count
    // [4..5] uint16_t med_count
    // [6..7] uint16_t full_count
    // [8..11] uint32_t pent_wire_count
    static constexpr size_t LGZ_HDR_BYTES = 12u;
    size_t total_sz = LGZ_HDR_BYTES + zero_mask_bytes + quad_bytes + pent_wire_count + med_count + full_count;
    if (total_sz >= src_len || total_sz > dst_cap) {
        return false;
    }

    uint16_t h_orig = static_cast<uint16_t>(n);
    uint16_t h_small = static_cast<uint16_t>(small_u_count);
    uint16_t h_med = static_cast<uint16_t>(med_count);
    uint16_t h_full = static_cast<uint16_t>(full_count);
    uint32_t h_pent = static_cast<uint32_t>(pent_wire_count);

    size_t pos = 0;
    std::memcpy(dst + pos, &h_orig, sizeof(h_orig)); pos += sizeof(h_orig);
    std::memcpy(dst + pos, &h_small, sizeof(h_small)); pos += sizeof(h_small);
    std::memcpy(dst + pos, &h_med, sizeof(h_med)); pos += sizeof(h_med);
    std::memcpy(dst + pos, &h_full, sizeof(h_full)); pos += sizeof(h_full);
    std::memcpy(dst + pos, &h_pent, sizeof(h_pent)); pos += sizeof(h_pent);

    std::memcpy(dst + pos, zero_mask.data(), zero_mask_bytes); pos += zero_mask_bytes;
    std::memcpy(dst + pos, quad_stream.data(), quad_bytes); pos += quad_bytes;
    if (pent_wire_count > 0) {
        std::memcpy(dst + pos, pent_small_buf.data(), pent_wire_count); pos += pent_wire_count;
    }
    if (med_count > 0) {
        std::memcpy(dst + pos, med_buf.data(), med_count); pos += med_count;
    }
    if (full_count > 0) {
        std::memcpy(dst + pos, full_buf.data(), full_count); pos += full_count;
    }

    out_len = pos;
    return true;
}

inline bool lgz345_decode_stream(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dst,
    size_t dst_cap,
    size_t& out_len
) noexcept {
    static constexpr size_t LGZ_HDR_BYTES = 12u;
    if (!src || !dst || src_len < LGZ_HDR_BYTES) return false;

    uint16_t h_orig = 0, h_small = 0, h_med = 0, h_full = 0;
    uint32_t h_pent = 0;
    size_t pos = 0;
    std::memcpy(&h_orig, src + pos, sizeof(h_orig)); pos += sizeof(h_orig);
    std::memcpy(&h_small, src + pos, sizeof(h_small)); pos += sizeof(h_small);
    std::memcpy(&h_med, src + pos, sizeof(h_med)); pos += sizeof(h_med);
    std::memcpy(&h_full, src + pos, sizeof(h_full)); pos += sizeof(h_full);
    std::memcpy(&h_pent, src + pos, sizeof(h_pent)); pos += sizeof(h_pent);

    size_t n = static_cast<size_t>(h_orig);
    if (n == 0 || n > dst_cap || n > 65535u) return false;

    size_t zero_mask_bytes = (n + 7u) / 8u;
    size_t quad_bytes = (n + 3u) / 4u;
    size_t expected_pent = (static_cast<size_t>(h_small) + 2u) / 3u;
    if (static_cast<size_t>(h_pent) != expected_pent) return false;

    size_t expected_total = LGZ_HDR_BYTES + zero_mask_bytes + quad_bytes +
                            static_cast<size_t>(h_pent) + static_cast<size_t>(h_med) + static_cast<size_t>(h_full);
    if (expected_total != src_len) return false;

    static thread_local alignas(64) std::array<uint8_t, 8192> zero_mask{};
    static thread_local alignas(64) std::array<uint8_t, 16384> quad_stream{};
    static thread_local alignas(64) std::array<uint8_t, 65536> small_u_vals{};

    if (zero_mask_bytes > zero_mask.size() || quad_bytes > quad_stream.size()) return false;

    std::memset(zero_mask.data(), 0, zero_mask_bytes);
    std::memset(quad_stream.data(), 0, quad_bytes);

    std::memcpy(zero_mask.data(), src + pos, zero_mask_bytes); pos += zero_mask_bytes;
    std::memcpy(quad_stream.data(), src + pos, quad_bytes); pos += quad_bytes;
    const uint8_t* pent_stream = src + pos; pos += static_cast<size_t>(h_pent);
    const uint8_t* med_stream = src + pos; pos += static_cast<size_t>(h_med);
    const uint8_t* full_stream = src + pos; pos += static_cast<size_t>(h_full);

    size_t small_produced = 0;
    for (size_t k = 0; k < static_cast<size_t>(h_pent); ++k) {
        unsigned b = static_cast<unsigned>(pent_stream[k]);
        unsigned u0 = b % 5u;
        unsigned u1 = (b / 5u) % 5u;
        unsigned u2 = (b / 25u) % 5u;
        if (small_produced < static_cast<size_t>(h_small)) {
            small_u_vals[small_produced++] = static_cast<uint8_t>(u0);
        }
        if (small_produced < static_cast<size_t>(h_small)) {
            small_u_vals[small_produced++] = static_cast<uint8_t>(u1);
        }
        if (small_produced < static_cast<size_t>(h_small)) {
            small_u_vals[small_produced++] = static_cast<uint8_t>(u2);
        }
    }
    if (small_produced != static_cast<size_t>(h_small)) return false;

    size_t small_idx = 0;
    size_t med_idx = 0;
    size_t full_idx = 0;

    for (size_t i = 0; i < n; ++i) {
        bool is_nonzero = ((zero_mask[i >> 3] >> (i & 7u)) & 1u) != 0u;
        if (!is_nonzero) {
            dst[i] = 0u;
            continue;
        }
        unsigned q_code = static_cast<unsigned>((quad_stream[i >> 2] >> ((i & 3u) * 2u)) & 0x3u);
        if (q_code == 0u) {
            if (small_idx >= static_cast<size_t>(h_small)) return false;
            int iv = static_cast<int>(small_u_vals[small_idx++]) - 2;
            dst[i] = static_cast<uint8_t>(static_cast<int8_t>(iv));
        } else if (q_code == 1u) {
            if (med_idx >= static_cast<size_t>(h_med)) return false;
            dst[i] = med_stream[med_idx++];
        } else if (q_code == 2u) {
            if (full_idx >= static_cast<size_t>(h_full)) return false;
            dst[i] = full_stream[full_idx++];
        } else {
            return false;
        }
    }

    if (small_idx != static_cast<size_t>(h_small) ||
        med_idx != static_cast<size_t>(h_med) ||
        full_idx != static_cast<size_t>(h_full)) {
        return false;
    }

    out_len = n;
    return true;
}

} // namespace ZvdNestedCodecDetail
// =====================================================================
// SECTION 5: COUPLED MICRO-TETRA TAGS, ANCHORS & MOIRE KEY DERIVATION
// =====================================================================

namespace ZvdNestedCodecDetail {

inline uint32_t compute_coupled_micro_tag32(
    uint16_t geo_id,
    uint16_t vertex_base5_code,
    uint16_t slot_mask,
    uint16_t raw_micro_bytes,
    uint32_t stored_len_and_flags,
    const CubeLevel2Anchor& cube_anchor,
    const uint8_t* wire_blob,
    size_t wire_len
) noexcept {
    int8_t v_p5[4];
    unpack_vertices_base5(vertex_base5_code, v_p5);

    uint64_t edge_coupling = 0ULL;
    for (size_t e = 0; e < ZVD_EDGE_SLOTS_COUNT; ++e) {
        uint8_t ia = TETRA_EDGE_PAIRS[e][0];
        uint8_t ib = TETRA_EDGE_PAIRS[e][1];
        int va = static_cast<int>(v_p5[ia]) + 3;
        int vb = static_cast<int>(v_p5[ib]) + 3;
        int hyp345 = 3 * va + 4 * vb;
        edge_coupling ^= (static_cast<uint64_t>(hyp345) * (0x9E3779B97F4A7C15ULL + (e + 1u) * 0xBF58476D1CE4E5B9ULL));
    }

    uint64_t cube_hash = zvd_fast_hash64(cube_anchor.data, ZVD_CUBE_ANCHOR_BYTES);
    uint64_t blob_hash = zvd_fast_hash64(wire_blob, wire_len);
    uint64_t meta = (static_cast<uint64_t>(geo_id) << 48) ^
                    (static_cast<uint64_t>(vertex_base5_code) << 32) ^
                    (static_cast<uint64_t>(slot_mask) << 16) ^
                    static_cast<uint64_t>(raw_micro_bytes) ^
                    (static_cast<uint64_t>(stored_len_and_flags) * 0xFF51AFD7ED558CCDULL);

    uint64_t h = blob_hash ^ meta ^ edge_coupling ^ (cube_hash * 0x9E3779B97F4A7C15ULL);
    h ^= (h >> 33);
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= (h >> 33);
    return static_cast<uint32_t>(h & 0xFFFFFFFFULL);
}

inline void build_coupled_slot_key(
    const char* master_key,
    ZvdCodecTopologyMode mode,
    uint16_t geo_id,
    uint16_t slot_idx,
    const int8_t vertices_p5[4],
    char out_key[128]
) noexcept {
    const char* base_k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
    if (slot_idx < ZVD_VERTEX_SLOTS_COUNT) {
        int v_val = static_cast<int>(vertices_p5[slot_idx]);
        std::snprintf(out_key, 128, "%s|M%u|G%u|V%u|P%d",
                      base_k, static_cast<unsigned>(mode), static_cast<unsigned>(geo_id),
                      static_cast<unsigned>(slot_idx), v_val);
    } else {
        size_t e_idx = static_cast<size_t>(slot_idx - ZVD_VERTEX_SLOTS_COUNT);
        if (e_idx >= ZVD_EDGE_SLOTS_COUNT) e_idx = 0;
        uint8_t ia = TETRA_EDGE_PAIRS[e_idx][0];
        uint8_t ib = TETRA_EDGE_PAIRS[e_idx][1];
        int va = static_cast<int>(vertices_p5[ia]);
        int vb = static_cast<int>(vertices_p5[ib]);
        int hyp_345 = 3 * (va + 3) + 4 * (vb + 3);
        std::snprintf(out_key, 128, "%s|M%u|G%u|E%u|VA%d|VB%d|H%d",
                      base_k, static_cast<unsigned>(mode), static_cast<unsigned>(geo_id),
                      static_cast<unsigned>(slot_idx), va, vb, hyp_345);
    }
}

inline uint64_t compute_header_integrity_hash(const MacroCellDiskHeader& hdr) noexcept {
    MacroCellDiskHeader tmp = hdr;
    tmp.header_hash64 = 0;
    tmp.cubes_hash64 = 0;
    tmp.directory_hash64 = 0;
    return zvd_fast_hash64(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

inline void build_icosa_anchor_120(
    const uint8_t* input_data,
    size_t input_len,
    uint64_t root_hash64,
    uint8_t out_icosa_120[ZVD_ICOSA_ANCHOR_BYTES]
) noexcept {
    std::array<uint64_t, ZVD_ICOSA_ANCHOR_BYTES> acc{};
    std::array<uint64_t, ZVD_ICOSA_ANCHOR_BYTES> cnt{};
    acc.fill(0);
    cnt.fill(0);

    size_t stride = (input_len > 1048576u) ? (input_len / 1048576u) : 1u;
    for (size_t i = 0; i < input_len; i += stride) {
        size_t idx = i % ZVD_ICOSA_ANCHOR_BYTES;
        acc[idx] += static_cast<uint64_t>(input_data[i]);
        cnt[idx] += 1ULL;
    }

    for (size_t k = 0; k < ZVD_ICOSA_ANCHOR_BYTES; ++k) {
        uint64_t mix_seed = root_hash64 ^ (static_cast<uint64_t>(k) * 0x9E3779B97F4A7C15ULL);
        mix_seed ^= (mix_seed >> 27);
        uint8_t fallback = static_cast<uint8_t>((mix_seed * 0x2545F4914F6CDD1DULL) >> 56);
        if (cnt[k] > 0) {
            uint8_t mean_b = static_cast<uint8_t>((acc[k] / cnt[k]) & 0xFFULL);
            out_icosa_120[k] = (k < 60u) ? mean_b : static_cast<uint8_t>(mean_b ^ (fallback & 0x1Fu));
        } else {
            out_icosa_120[k] = fallback;
        }
    }
}

inline void build_cube_anchor_14(
    const uint8_t* cube_data,
    size_t cube_len,
    const uint8_t icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    uint16_t cube_idx,
    CubeLevel2Anchor& out_cube
) noexcept {
    std::array<uint64_t, ZVD_CUBE_ANCHOR_BYTES> acc{};
    std::array<uint64_t, ZVD_CUBE_ANCHOR_BYTES> cnt{};
    acc.fill(0);
    cnt.fill(0);

    if (cube_data && cube_len > 0) {
        for (size_t i = 0; i < cube_len; ++i) {
            size_t b_idx = i % ZVD_CUBE_ANCHOR_BYTES;
            acc[b_idx] += static_cast<uint64_t>(cube_data[i]);
            cnt[b_idx] += 1ULL;
        }
    }

    for (size_t j = 0; j < ZVD_CUBE_ANCHOR_BYTES; ++j) {
        uint8_t i_ref = icosa_120[(static_cast<size_t>(cube_idx) * ZVD_CUBE_ANCHOR_BYTES + j) % ZVD_ICOSA_ANCHOR_BYTES];
        if (cnt[j] > 0) {
            out_cube.data[j] = static_cast<uint8_t>((acc[j] / cnt[j]) & 0xFFULL);
        } else {
            out_cube.data[j] = static_cast<uint8_t>(i_ref ^ static_cast<uint8_t>(cube_idx + j));
        }
    }
}

} // namespace ZvdNestedCodecDetail
// =====================================================================
// SECTION 6: MICRO-TETRA PACKING & UNPACKING (PLANE-SPLIT + LGZ-345 + PYTH-345 + LZ4 + MOIRE)
// =====================================================================

namespace ZvdNestedCodecDetail {

inline bool pack_single_micro_tetra(
    uint16_t geo_id,
    uint16_t vertex_base5_code,
    const int8_t vertices_p5[4],
    const uint8_t icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    const CubeLevel2Anchor& cube_anchor,
    uint16_t slot_mask,
    const uint8_t slot_raw_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES],
    const uint16_t slot_raw_lens[ZVD_MICRO_SLOTS_COUNT],
    const char* master_key,
    ZvdCodecTopologyMode mode,
    uint8_t* dst_blob,
    size_t dst_cap,
    uint16_t& out_raw_micro_bytes,
    uint32_t& out_stored_len_and_flags,
    uint32_t& out_coupled_micro_tag32,
    uint32_t& out_wire_bytes
) noexcept {
    if (!dst_blob || dst_cap < ZVD_MICRO_LEN_TABLE_BYTES || vertex_base5_code > ZVD3_MAX_BASE5_CODE) {
        return false;
    }

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES> raw_concat{};
    uint16_t slot_len_table[ZVD_MICRO_SLOTS_COUNT]{0};
    uint32_t raw_total = 0;

    for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
        if ((slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
            uint16_t slen = slot_raw_lens[s];
            if (slen == 0 || raw_total + slen > raw_concat.size()) return false;

            std::memcpy(raw_concat.data() + raw_total, slot_raw_slices[s], slen);
            slot_len_table[s] = slen;
            raw_total += slen;
        }
    }

    if (raw_total == 0 || raw_total > 65535u) return false;

    // Step A: Orthogonal 4-plane split on raw micro-tetra buffer
    bool use_plane_split = (raw_total >= 40u);
    if (use_plane_split) {
        tetra_plane_split_forward(raw_concat.data(), raw_total);
    }

    // Step B: Adaptive LGZ-345 cascade before PYTH-345 mixing
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 2048> lgz_out{};
    size_t lgz_sz = 0;
    bool use_lgz345 = (raw_total >= 16u) &&
                      lgz345_encode_adaptive(raw_concat.data(), raw_total, lgz_out.data(), lgz_out.size(), lgz_sz);

    // Step C: Apply PYTH-345 transform after LGZ-345 (and before LZ4 + 120D Moire)
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 2048> post_pyth_buf{};
    const uint8_t* pre_lz4_ptr = nullptr;
    size_t pre_lz4_len = 0;
    bool use_post_pyth = false;

    if (use_lgz345) {
        if (lgz_sz > post_pyth_buf.size()) return false;
        std::memcpy(post_pyth_buf.data(), lgz_out.data(), lgz_sz);
        pyth_345_transform_buffer(post_pyth_buf.data(), lgz_sz, icosa_120, cube_anchor, true);
        pre_lz4_ptr = post_pyth_buf.data();
        pre_lz4_len = lgz_sz;
        use_post_pyth = true;
    } else {
        pre_lz4_ptr = raw_concat.data();
        pre_lz4_len = static_cast<size_t>(raw_total);
        use_post_pyth = false;
    }

    // Step D: Stack LZ4 (when entropy below threshold) + 120D Moire + coupled integrity tag
    static thread_local alignas(64) std::array<uint8_t, ZVD_MICRO_LEN_TABLE_BYTES + ZVD_MAX_MICRO_RAW_BYTES + 4096> staging{};
    std::memcpy(staging.data(), slot_len_table, ZVD_MICRO_LEN_TABLE_BYTES);

    size_t body_wire_len = pre_lz4_len;
    bool compressed = false;

    if (pre_lz4_len >= 32u) {
        size_t sample_len = (pre_lz4_len < 512u) ? pre_lz4_len : 512u;
        double ent = zvd_shannon_entropy(pre_lz4_ptr, sample_len);
        if (ent < ZVD_ENTROPY_BYPASS_THRESHOLD) {
            static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 4096> lz4_out{};
            size_t c_sz = zvd_lz4_compress_block_stack(pre_lz4_ptr, pre_lz4_len, lz4_out.data(), lz4_out.size());
            if (c_sz > 0u && c_sz < pre_lz4_len && (ZVD_MICRO_LEN_TABLE_BYTES + c_sz) <= staging.size()) {
                std::memcpy(staging.data() + ZVD_MICRO_LEN_TABLE_BYTES, lz4_out.data(), c_sz);
                body_wire_len = c_sz;
                compressed = true;
            }
        }
    }

    if (!compressed) {
        if (ZVD_MICRO_LEN_TABLE_BYTES + pre_lz4_len > staging.size()) return false;
        std::memcpy(staging.data() + ZVD_MICRO_LEN_TABLE_BYTES, pre_lz4_ptr, pre_lz4_len);
        body_wire_len = pre_lz4_len;
    }

    char micro_key[128];
    build_coupled_slot_key(master_key, mode, geo_id, 0u, vertices_p5, micro_key);
    zvd_120d_moire_crypt_buffer(staging.data() + ZVD_MICRO_LEN_TABLE_BYTES, body_wire_len, micro_key, static_cast<size_t>(geo_id), true);

    size_t total_wire = ZVD_MICRO_LEN_TABLE_BYTES + body_wire_len;
    if (total_wire > dst_cap || total_wire > ZVD3_SLOT_SIZE_MASK) return false;

    std::memcpy(dst_blob, staging.data(), total_wire);

    uint32_t flags = (compressed ? ZVD3_SLOT_FLAG_COMPRESSED : 0u) |
                     (use_plane_split ? ZVD3_SLOT_FLAG_PLANE_SPLIT : 0u) |
                     (use_lgz345 ? ZVD3_SLOT_FLAG_LGZ345 : 0u) |
                     (use_post_pyth ? ZVD3_SLOT_FLAG_POST_PYTH : 0u);
    out_raw_micro_bytes = static_cast<uint16_t>(raw_total);
    out_stored_len_and_flags = static_cast<uint32_t>(total_wire) | flags;
    out_coupled_micro_tag32 = compute_coupled_micro_tag32(
        geo_id, vertex_base5_code, slot_mask, out_raw_micro_bytes,
        out_stored_len_and_flags, cube_anchor, dst_blob, total_wire
    );
    out_wire_bytes = static_cast<uint32_t>(total_wire);
    return true;
}

inline bool unpack_single_micro_tetra(
    uint16_t geo_id,
    uint16_t vertex_base5_code,
    uint16_t slot_mask,
    uint16_t raw_micro_bytes,
    uint32_t stored_len_and_flags,
    uint32_t expected_coupled_tag32,
    const uint8_t icosa_120[ZVD_ICOSA_ANCHOR_BYTES],
    const CubeLevel2Anchor& cube_anchor,
    const uint8_t* wire_blob,
    size_t wire_len,
    const char* master_key,
    ZvdCodecTopologyMode mode,
    uint8_t out_slot_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES],
    uint16_t out_slot_lens[ZVD_MICRO_SLOTS_COUNT]
) noexcept {
    if (!wire_blob || wire_len <= ZVD_MICRO_LEN_TABLE_BYTES || vertex_base5_code > ZVD3_MAX_BASE5_CODE) {
        return false;
    }

    uint32_t actual_tag32 = compute_coupled_micro_tag32(
        geo_id, vertex_base5_code, slot_mask, raw_micro_bytes,
        stored_len_and_flags, cube_anchor, wire_blob, wire_len
    );
    if (actual_tag32 != expected_coupled_tag32) {
        return false;
    }

    int8_t v_p5[4];
    unpack_vertices_base5(vertex_base5_code, v_p5);

    uint16_t slot_len_table[ZVD_MICRO_SLOTS_COUNT]{0};
    std::memcpy(slot_len_table, wire_blob, ZVD_MICRO_LEN_TABLE_BYTES);

    size_t body_wire_len = wire_len - ZVD_MICRO_LEN_TABLE_BYTES;
    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 4096> body_crypt{};
    if (body_wire_len > body_crypt.size()) return false;
    std::memcpy(body_crypt.data(), wire_blob + ZVD_MICRO_LEN_TABLE_BYTES, body_wire_len);

    // 1. Decrypt 120D Moire layer
    char micro_key[128];
    build_coupled_slot_key(master_key, mode, geo_id, 0u, v_p5, micro_key);
    zvd_120d_moire_crypt_buffer(body_crypt.data(), body_wire_len, micro_key, static_cast<size_t>(geo_id), false);

    bool compressed = (stored_len_and_flags & ZVD3_SLOT_FLAG_COMPRESSED) != 0u;
    bool egypt345_set = (stored_len_and_flags & ZVD3_SLOT_FLAG_EGYPT345) != 0u;
    bool plane_split_set = (stored_len_and_flags & ZVD3_SLOT_FLAG_PLANE_SPLIT) != 0u;
    bool lgz345_set = (stored_len_and_flags & ZVD3_SLOT_FLAG_LGZ345) != 0u;
    bool post_pyth_set = (stored_len_and_flags & ZVD3_SLOT_FLAG_POST_PYTH) != 0u;

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 4096> after_lz4{};
    size_t after_lz4_len = 0;

    // 2. Decompress LZ4 if flagged
    if (!compressed) {
        if (body_wire_len > after_lz4.size()) return false;
        std::memcpy(after_lz4.data(), body_crypt.data(), body_wire_len);
        after_lz4_len = body_wire_len;
    } else {
        if (!zvd_lz4_decompress_block_safe(body_crypt.data(), body_wire_len, after_lz4.data(), after_lz4.size(), &after_lz4_len)) {
            return false;
        }
    }

    static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES> raw_concat{};
    size_t recovered_raw_len = 0;

    if (post_pyth_set) {
        // Mirror order: 120D Moire -> LZ4 -> PYTH-345 inverse -> LGZ-345 decode -> tetra_plane_split_inverse
        if (!lgz345_set || after_lz4_len > raw_concat.size()) return false;
        static thread_local alignas(64) std::array<uint8_t, ZVD_MAX_MICRO_RAW_BYTES + 2048> unpyth_buf{};
        if (after_lz4_len > unpyth_buf.size()) return false;
        std::memcpy(unpyth_buf.data(), after_lz4.data(), after_lz4_len);
        pyth_345_transform_buffer(unpyth_buf.data(), after_lz4_len, icosa_120, cube_anchor, false);

        if (!lgz345_decode_stream(unpyth_buf.data(), after_lz4_len, raw_concat.data(), raw_concat.size(), recovered_raw_len)) {
            return false;
        }
        if (recovered_raw_len != static_cast<size_t>(raw_micro_bytes)) {
            return false;
        }
        if (plane_split_set) {
            tetra_plane_split_inverse(raw_concat.data(), recovered_raw_len);
        }
    } else {
        // Legacy order for slots without ZVD3_SLOT_FLAG_POST_PYTH
        if (lgz345_set) {
            if (!lgz345_decode_stream(after_lz4.data(), after_lz4_len, raw_concat.data(), raw_concat.size(), recovered_raw_len)) {
                return false;
            }
        } else {
            if (egypt345_set && after_lz4_len >= 3u) {
                int pyth_lock = zvd_pythagorean_directive_lock(static_cast<int>((geo_id % 19u) + 1u));
                uint16_t s_idx = static_cast<uint16_t>(vertex_base5_code & 0xFFu);
                uint8_t k3 = static_cast<uint8_t>((3u * (static_cast<uint32_t>(s_idx) + 1u) + static_cast<uint32_t>(std::abs(pyth_lock))) & 0xFFu);
                uint8_t k4 = static_cast<uint8_t>((4u * (static_cast<uint32_t>(geo_id) + 1u) + static_cast<uint32_t>(s_idx * 3u)) & 0xFFu);
                uint8_t k5 = static_cast<uint8_t>((5u * (static_cast<uint32_t>(geo_id + s_idx) + 1u)) & 0xFFu);
                size_t full_triples = (after_lz4_len / 3u) * 3u;
                for (size_t i = full_triples; i >= 3u; i -= 3u) {
                    size_t base = i - 3u;
                    uint8_t na = after_lz4[base];
                    uint8_t nb = after_lz4[base + 1u];
                    uint8_t hyp_c = after_lz4[base + 2u];
                    uint8_t a = static_cast<uint8_t>(na - static_cast<uint8_t>(4u * nb) - k3);
                    uint8_t b = static_cast<uint8_t>(nb - static_cast<uint8_t>(3u * a) - k4);
                    uint8_t c = static_cast<uint8_t>(hyp_c - static_cast<uint8_t>(3u * a + 4u * b) - k5);
                    after_lz4[base] = a;
                    after_lz4[base + 1u] = b;
                    after_lz4[base + 2u] = c;
                    if (base == 0) break;
                }
            }
            if (after_lz4_len > raw_concat.size()) return false;
            std::memcpy(raw_concat.data(), after_lz4.data(), after_lz4_len);
            recovered_raw_len = after_lz4_len;
        }

        if (recovered_raw_len != static_cast<size_t>(raw_micro_bytes)) {
            return false;
        }

        if (plane_split_set) {
            tetra_plane_split_inverse(raw_concat.data(), recovered_raw_len);
        }

        pyth_345_transform_buffer(raw_concat.data(), recovered_raw_len, icosa_120, cube_anchor, false);
    }

    size_t pos = 0;
    for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
        out_slot_lens[s] = 0;
        if ((slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
            uint16_t slen = slot_len_table[s];
            if (slen == 0 || slen > ZVD_STELLA8_SLOT_BYTES || pos + slen > recovered_raw_len) {
                return false;
            }
            std::memcpy(out_slot_slices[s], raw_concat.data() + pos, slen);
            out_slot_lens[s] = slen;
            pos += slen;
        }
    }

    return (pos == recovered_raw_len);
}

} // namespace ZvdNestedCodecDetail
// =====================================================================
// SECTION 7: MACRO-CELL ENCODING, 3-LEVEL FOLDING & RESONANCE SCORING
// =====================================================================

inline bool zvd_nested_encode_macro_cell(
    const uint8_t* input_data,
    size_t input_len,
    const char* master_key,
    ZvdCodecTopologyMode mode,
    MacroCellWorkspace& out_cell
) noexcept {
    if (!input_data || input_len == 0 || input_len > ZVD_MAX_MACRO_CELL_BYTES) {
        return false;
    }
    zvd_ensure_luts();
    ZvdNestedCodecDetail::ensure_pyth_345_lut();

    ZvdNestedCodecDetail::geometry_dims_for_mode(
        mode, out_cell.l1_tetras, out_cell.l2_cubes, out_cell.l3_micros, out_cell.slot_bytes
    );

    const size_t total_slots = out_cell.l3_micros * ZVD_MICRO_SLOTS_COUNT;
    const size_t max_cell_capacity = total_slots * out_cell.slot_bytes;
    if (input_len > max_cell_capacity) {
        return false;
    }

    out_cell.mode = mode;
    out_cell.active_cube_count = 0;
    out_cell.active_micro_count = 0;
    out_cell.raw_payload_bytes = input_len;
    out_cell.payload_used = 0;
    out_cell.root_hash64 = zvd_fast_hash64(input_data, input_len);

    // Level 1: Global 120D Icosahedron anchor
    ZvdNestedCodecDetail::build_icosa_anchor_120(
        input_data, input_len, out_cell.root_hash64, out_cell.icosa_anchor_120.data()
    );

    // Level 2: Per-cube 14-byte anchors (12 micro-tetras per cube)
    size_t bytes_per_cube = ZVD_STELLA8_MICROS_PER_CUBE * ZVD_MICRO_SLOTS_COUNT * out_cell.slot_bytes;
    size_t needed_cubes = (input_len + bytes_per_cube - 1u) / bytes_per_cube;
    if (needed_cubes > out_cell.l2_cubes) needed_cubes = out_cell.l2_cubes;
    out_cell.active_cube_count = needed_cubes;

    for (size_t c = 0; c < out_cell.l2_cubes; ++c) {
        size_t c_start = c * bytes_per_cube;
        if (c_start < input_len) {
            size_t c_len = std::min(bytes_per_cube, input_len - c_start);
            ZvdNestedCodecDetail::build_cube_anchor_14(
                input_data + c_start, c_len, out_cell.icosa_anchor_120.data(),
                static_cast<uint16_t>(c), out_cell.cubes[c]
            );
        } else {
            for (size_t j = 0; j < ZVD_CUBE_ANCHOR_BYTES; ++j) {
                out_cell.cubes[c].data[j] = out_cell.icosa_anchor_120[(c * ZVD_CUBE_ANCHOR_BYTES + j) % ZVD_ICOSA_ANCHOR_BYTES];
            }
        }
    }

    std::array<int8_t, ZVD_CELLS_PER_LAYER> folded_500{};
    folded_500.fill(0);

    size_t cursor = 0;
    size_t dir_idx = 0;

    for (uint16_t geo_id = 0; geo_id < static_cast<uint16_t>(out_cell.l3_micros); ++geo_id) {
        if (cursor >= input_len) break;

        uint8_t slot_raw_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES]{};
        uint16_t slot_raw_lens[ZVD_MICRO_SLOTS_COUNT]{0};
        uint16_t slot_mask = 0;
        int8_t v_p5[ZVD_VERTEX_SLOTS_COUNT]{0, 0, 0, 0};

        for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
            if (cursor >= input_len) break;
            size_t take = std::min(out_cell.slot_bytes, input_len - cursor);
            std::memcpy(slot_raw_slices[s], input_data + cursor, take);
            slot_raw_lens[s] = static_cast<uint16_t>(take);
            slot_mask = static_cast<uint16_t>(slot_mask | static_cast<uint16_t>(1u << s));
            cursor += take;

            if (s < ZVD_VERTEX_SLOTS_COUNT) {
                v_p5[s] = ZvdNestedCodecDetail::derive_vertex_p5_from_slot(
                    slot_raw_slices[s], take, geo_id, s
                );
            }
        }

        for (uint16_t v = 0; v < ZVD_VERTEX_SLOTS_COUNT; ++v) {
            if ((slot_mask & static_cast<uint16_t>(1u << v)) == 0) {
                uint64_t h = out_cell.root_hash64 ^ (static_cast<uint64_t>(geo_id) * 2654435761ULL) ^ v;
                v_p5[v] = static_cast<int8_t>((h % 5ULL) - 2);
            }
        }

        uint16_t v_code = ZvdNestedCodecDetail::pack_vertices_base5(v_p5[0], v_p5[1], v_p5[2], v_p5[3]);
        if (out_cell.payload_used + ZVD_MICRO_LEN_TABLE_BYTES + ZVD_MAX_MICRO_RAW_BYTES + 4096u > out_cell.payload_arena.size()) {
            return false;
        }

        size_t cube_idx = static_cast<size_t>(geo_id) / ZVD_STELLA8_MICROS_PER_CUBE;
        if (cube_idx >= out_cell.l2_cubes) cube_idx = out_cell.l2_cubes - 1u;

        uint16_t raw_micro_b = 0;
        uint32_t stored_flags = 0;
        uint32_t tag32 = 0;
        uint32_t wire_b = 0;

        if (!ZvdNestedCodecDetail::pack_single_micro_tetra(
                geo_id,
                v_code,
                v_p5,
                out_cell.icosa_anchor_120.data(),
                out_cell.cubes[cube_idx],
                slot_mask,
                slot_raw_slices,
                slot_raw_lens,
                master_key,
                mode,
                out_cell.payload_arena.data() + out_cell.payload_used,
                out_cell.payload_arena.size() - out_cell.payload_used,
                raw_micro_b,
                stored_flags,
                tag32,
                wire_b)) {
            return false;
        }

        MicroTetraDirectoryEntry entry{};
        entry.geo_id = geo_id;
        entry.vertex_base5_code = v_code;
        entry.slot_mask = slot_mask;
        entry.raw_micro_bytes = raw_micro_b;
        entry.stored_len_and_flags = stored_flags;
        entry.coupled_micro_tag32 = tag32;
        entry.file_offset = 0;

        out_cell.payload_used += static_cast<size_t>(wire_b);
        out_cell.directory[dir_idx++] = entry;

        size_t target_idx = static_cast<size_t>(geo_id) % ZVD_CELLS_PER_LAYER;
        int fold_acc = static_cast<int>(folded_500[target_idx]) +
                       static_cast<int>(v_p5[0]) + static_cast<int>(v_p5[1]) -
                       static_cast<int>(v_p5[2]) - static_cast<int>(v_p5[3]);
        folded_500[target_idx] = zvd_lukasiewicz_clamp(fold_acc);
    }

    out_cell.active_micro_count = dir_idx;
    zvd_pack_layer_lukasiewicz(folded_500.data(), out_cell.root_monolith.face_packed);

    std::array<int8_t, ZVD_CELLS_PER_LAYER> topo_500{};
    for (size_t i = 0; i < ZVD_CELLS_PER_LAYER; ++i) {
        int val = static_cast<int>(folded_500[i]);
        if (out_cell.active_micro_count > 0) {
            uint16_t m_code = out_cell.directory[i % out_cell.active_micro_count].vertex_base5_code;
            int8_t uv[4];
            ZvdNestedCodecDetail::unpack_vertices_base5(m_code, uv);
            val += (static_cast<int>(uv[i & 3]) - static_cast<int>(uv[(i + 1) & 3]));
        }
        topo_500[i] = zvd_lukasiewicz_clamp(val);
    }
    zvd_pack_layer_lukasiewicz(topo_500.data(), out_cell.root_monolith.vertex_packed);

    int8_t vec120[ZVD_DIM_120];
    for (int i = 0; i < ZVD_DIM_120; ++i) {
        vec120[i] = folded_500[i];
    }
    out_cell.root_gl_invariant = zvd_godel_lukasiewicz_reduce(vec120);
    return true;
}

inline bool zvd_nested_decode_macro_cell(
    const MacroCellWorkspace& cell,
    const char* master_key,
    uint8_t* out_data,
    size_t out_cap,
    size_t* out_len,
    bool* out_bit_perfect
) noexcept {
    if (!out_data || !out_len || out_cap < cell.raw_payload_bytes) {
        if (out_bit_perfect) *out_bit_perfect = false;
        return false;
    }
    if (cell.payload_used > cell.payload_arena.size()) {
        if (out_bit_perfect) *out_bit_perfect = false;
        return false;
    }

    size_t arena_pos = 0;
    size_t write_pos = 0;

    for (size_t d = 0; d < cell.active_micro_count; ++d) {
        const MicroTetraDirectoryEntry& entry = cell.directory[d];
        uint32_t wire_only = entry.stored_len_and_flags & ZVD3_SLOT_SIZE_MASK;
        if (arena_pos + wire_only > cell.payload_used) {
            if (out_bit_perfect) *out_bit_perfect = false;
            return false;
        }

        size_t cube_idx = static_cast<size_t>(entry.geo_id) / ZVD_STELLA8_MICROS_PER_CUBE;
        if (cube_idx >= cell.l2_cubes) cube_idx = cell.l2_cubes - 1u;

        uint8_t slot_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES]{};
        uint16_t slot_lens[ZVD_MICRO_SLOTS_COUNT]{0};

        if (!ZvdNestedCodecDetail::unpack_single_micro_tetra(
                entry.geo_id,
                entry.vertex_base5_code,
                entry.slot_mask,
                entry.raw_micro_bytes,
                entry.stored_len_and_flags,
                entry.coupled_micro_tag32,
                cell.icosa_anchor_120.data(),
                cell.cubes[cube_idx],
                cell.payload_arena.data() + arena_pos,
                wire_only,
                master_key,
                cell.mode,
                slot_slices,
                slot_lens)) {
            if (out_bit_perfect) *out_bit_perfect = false;
            return false;
        }

        for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
            if ((entry.slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
                uint16_t slen = slot_lens[s];
                if (write_pos + slen > out_cap) {
                    if (out_bit_perfect) *out_bit_perfect = false;
                    return false;
                }
                std::memcpy(out_data + write_pos, slot_slices[s], slen);
                write_pos += slen;
            }
        }

        arena_pos += wire_only;
    }

    uint64_t h = zvd_fast_hash64(out_data, write_pos);
    bool ok = (h == cell.root_hash64);
    *out_len = write_pos;
    if (out_bit_perfect) *out_bit_perfect = ok;
    return ok;
}

inline double zvd_nested_resonance_score(
    const ZVD_Monolith334& cell_monolith,
    int8_t cell_gl_inv,
    const ZVD_Monolith334& query_monolith,
    int8_t query_gl_inv
) noexcept {
    zvd_ensure_luts();
    uint64_t total_manhattan_diff = 0ULL;

    for (size_t i = 0; i < 166u; ++i) {
        uint8_t cf_code = cell_monolith.face_packed[i];
        uint8_t qf_code = query_monolith.face_packed[i];
        uint8_t cv_code = cell_monolith.vertex_packed[i];
        uint8_t qv_code = query_monolith.vertex_packed[i];
        if (cf_code >= 125u || qf_code >= 125u || cv_code >= 125u || qv_code >= 125u) {
            return 0.0;
        }
        for (size_t k = 0; k < 3u; ++k) {
            int cf = static_cast<int>(ZVD_UNPACK_BASE5_LUT[cf_code][k]);
            int qf = static_cast<int>(ZVD_UNPACK_BASE5_LUT[qf_code][k]);
            total_manhattan_diff += static_cast<uint64_t>(std::abs(cf - qf));

            int cv = static_cast<int>(ZVD_UNPACK_BASE5_LUT[cv_code][k]);
            int qv = static_cast<int>(ZVD_UNPACK_BASE5_LUT[qv_code][k]);
            total_manhattan_diff += static_cast<uint64_t>(std::abs(cv - qv));
        }
    }

    uint8_t cf_tail = cell_monolith.face_packed[166u];
    uint8_t qf_tail = query_monolith.face_packed[166u];
    uint8_t cv_tail = cell_monolith.vertex_packed[166u];
    uint8_t qv_tail = query_monolith.vertex_packed[166u];
    if (cf_tail >= 25u || qf_tail >= 25u || cv_tail >= 25u || qv_tail >= 25u) {
        return 0.0;
    }
    for (size_t k = 0; k < 2u; ++k) {
        int cf = static_cast<int>(ZVD_UNPACK_BASE5_LUT[cf_tail][k]);
        int qf = static_cast<int>(ZVD_UNPACK_BASE5_LUT[qf_tail][k]);
        total_manhattan_diff += static_cast<uint64_t>(std::abs(cf - qf));

        int cv = static_cast<int>(ZVD_UNPACK_BASE5_LUT[cv_tail][k]);
        int qv = static_cast<int>(ZVD_UNPACK_BASE5_LUT[qv_tail][k]);
        total_manhattan_diff += static_cast<uint64_t>(std::abs(cv - qv));
    }

    double base_sim = 1.0 - (static_cast<double>(total_manhattan_diff) / 4000.0);
    if (base_sim < 0.0) base_sim = 0.0;
    if (base_sim > 1.0) base_sim = 1.0;

    int inv_diff = std::abs(static_cast<int>(cell_gl_inv) - static_cast<int>(query_gl_inv));
    double inv_bonus = std::max(0.0, 1.0 - (static_cast<double>(inv_diff) / 4.0));
    return std::clamp(0.85 * base_sim + 0.15 * inv_bonus, 0.0, 1.0);
}
// =====================================================================
// SECTION 8: ATOMIC POSIX I/O HELPERS & .ZVD3 V4 WRITE / DEDUPLICATION
// =====================================================================

namespace ZvdNestedCodecDetail {

inline bool write_all_fd(int fd, const void* buf, size_t count) noexcept {
#ifdef _WIN32
    (void)fd; (void)buf; (void)count;
    return false;
#else
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    size_t rem = count;
    while (rem > 0) {
        ssize_t w = ::write(fd, p, rem);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (w == 0) return false;
        p += static_cast<size_t>(w);
        rem -= static_cast<size_t>(w);
    }
    return true;
#endif
}

inline bool pread_all_fd(int fd, void* buf, size_t count, off_t offset) noexcept {
#ifdef _WIN32
    (void)fd; (void)buf; (void)count; (void)offset;
    return false;
#else
    uint8_t* p = static_cast<uint8_t*>(buf);
    size_t rem = count;
    off_t cur_off = offset;
    while (rem > 0) {
        ssize_t r = ::pread(fd, p, rem, cur_off);
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (r == 0) return false;
        p += static_cast<size_t>(r);
        rem -= static_cast<size_t>(r);
        cur_off += static_cast<off_t>(r);
    }
    return true;
#endif
}

inline void ensure_vault_dir(const char* vault_dir) noexcept {
#ifndef _WIN32
    if (!vault_dir || vault_dir[0] == '\0') return;
    size_t len = std::strlen(vault_dir);
    if (len == 0 || len >= 512u) return;

    char path_buf[512];
    std::memcpy(path_buf, vault_dir, len + 1u);
    while (len > 1u && path_buf[len - 1u] == '/') {
        path_buf[--len] = '\0';
    }

    for (size_t i = 1u; i <= len; ++i) {
        if (path_buf[i] == '/' || path_buf[i] == '\0') {
            char saved = path_buf[i];
            path_buf[i] = '\0';
            if (path_buf[0] != '\0') {
                (void)::mkdir(path_buf, 0755);
            }
            path_buf[i] = saved;
        }
    }
#else
    (void)vault_dir;
#endif
}

inline void fsync_vault_dir(const char* vault_dir) noexcept {
#ifndef _WIN32
    if (!vault_dir || vault_dir[0] == '\0') return;
    int dfd = ::open(vault_dir, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        (void)::fsync(dfd);
        (void)::close(dfd);
    }
#else
    (void)vault_dir;
#endif
}

inline void build_cell_file_path(const char* vault_dir, ZvdCodecTopologyMode mode, uint64_t root_hash64, char out_path[512]) noexcept {
    std::snprintf(out_path, 512, "%s/cell_%s_%016llX.zvd3",
                  vault_dir, zvd_codec_mode_name(mode), static_cast<unsigned long long>(root_hash64));
}

inline bool read_and_verify_header(int fd, MacroCellDiskHeader& out_hdr, bool* out_veto) noexcept {
    if (!pread_all_fd(fd, &out_hdr, sizeof(out_hdr), 0)) {
        if (out_veto) *out_veto = true;
        return false;
    }
    if (out_hdr.magic != ZVD3_MAGIC || out_hdr.version != ZVD3_VERSION) {
        if (out_veto) *out_veto = true;
        return false;
    }
    if (out_hdr.header_hash64 != compute_header_integrity_hash(out_hdr)) {
        if (out_veto) *out_veto = true;
        return false;
    }
    return true;
}

} // namespace ZvdNestedCodecDetail

inline bool zvd_storage_put_nested_cell(
    const char* vault_dir,
    const uint8_t* data,
    size_t len,
    const char* master_key,
    ZvdCodecTopologyMode mode,
    char out_path[512],
    bool* out_deduplicated = nullptr
) noexcept {
#ifdef _WIN32
    (void)vault_dir; (void)data; (void)len; (void)master_key; (void)mode; (void)out_path; (void)out_deduplicated;
    return false;
#else
    if (out_deduplicated) *out_deduplicated = false;
    if (!vault_dir || !data || len == 0 || len > ZVD_MAX_MACRO_CELL_BYTES) return false;

    ZvdNestedCodecDetail::ensure_vault_dir(vault_dir);

    if (mode == ZvdCodecTopologyMode::FLAT_ZVDO) {
        if (len > ZVD_OPTIC_MAX_BLOCK_BYTES) return false;
        alignas(64) uint8_t optic_wire[sizeof(ZVD_OpticDatacenterHeader) + ZVD_OPTIC_MAX_BLOCK_BYTES + 1024];
        size_t optic_sz = 0;
        const char* k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
        if (!zvd_optic_frame_pack(101, 202, data, len, k, optic_wire, sizeof(optic_wire), &optic_sz)) {
            return false;
        }
        uint64_t h64 = zvd_fast_hash64(data, len);
        char final_path[512];
        ZvdNestedCodecDetail::build_cell_file_path(vault_dir, mode, h64, final_path);

        static std::atomic<uint64_t> flat_seq{0};
        uint64_t seq = flat_seq.fetch_add(1, std::memory_order_relaxed);
        char tmp_path[576];
        std::snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp.%ld.%llu.zvd3",
                      vault_dir, static_cast<long>(::getpid()), static_cast<unsigned long long>(seq));

        MacroCellDiskHeader hdr{};
        hdr.magic = ZVD3_MAGIC;
        hdr.version = ZVD3_VERSION;
        hdr.topology_mode = static_cast<uint8_t>(ZvdCodecTopologyMode::FLAT_ZVDO);
        hdr.raw_payload_bytes = static_cast<uint64_t>(len);
        hdr.packed_payload_bytes = static_cast<uint64_t>(optic_sz);
        hdr.root_hash64 = h64;
        hdr.cubes_hash64 = 0ULL;
        hdr.directory_hash64 = zvd_fast_hash64(optic_wire, optic_sz);
        hdr.root_gl_invariant = 0;
        hdr.active_cube_count = 0;
        hdr.cubes_offset = static_cast<uint32_t>(sizeof(MacroCellDiskHeader));
        hdr.active_micro_count = 0;
        hdr.directory_offset = static_cast<uint32_t>(sizeof(MacroCellDiskHeader));
        hdr.payload_base_offset = static_cast<uint32_t>(sizeof(MacroCellDiskHeader));
        hdr.header_hash64 = ZvdNestedCodecDetail::compute_header_integrity_hash(hdr);

        int fd = ::open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return false;
        bool ok = ZvdNestedCodecDetail::write_all_fd(fd, &hdr, sizeof(hdr)) &&
                  ZvdNestedCodecDetail::write_all_fd(fd, optic_wire, optic_sz);
        if (ok) (void)::fsync(fd);
        if (::close(fd) != 0) ok = false;
        if (!ok || ::rename(tmp_path, final_path) != 0) {
            (void)::unlink(tmp_path);
            return false;
        }
        ZvdNestedCodecDetail::fsync_vault_dir(vault_dir);
        if (out_path) std::snprintf(out_path, 512, "%s", final_path);
        return true;
    }

    static thread_local alignas(64) MacroCellWorkspace ws;
    if (!zvd_nested_encode_macro_cell(data, len, master_key, mode, ws)) {
        return false;
    }

    char final_path[512];
    ZvdNestedCodecDetail::build_cell_file_path(vault_dir, mode, ws.root_hash64, final_path);

    int existing_fd = ::open(final_path, O_RDONLY);
    if (existing_fd >= 0) {
        MacroCellDiskHeader existing_hdr{};
        bool existing_veto = false;
        bool ok_hdr = ZvdNestedCodecDetail::read_and_verify_header(existing_fd, existing_hdr, &existing_veto);
        ::close(existing_fd);
        if (ok_hdr &&
            existing_hdr.root_hash64 == ws.root_hash64 &&
            existing_hdr.raw_payload_bytes == static_cast<uint64_t>(len) &&
            std::memcmp(existing_hdr.icosa_anchor_120, ws.icosa_anchor_120.data(), ZVD_ICOSA_ANCHOR_BYTES) == 0 &&
            std::memcmp(existing_hdr.root_monolith.face_packed, ws.root_monolith.face_packed, ZVD_LAYER_BYTES) == 0 &&
            std::memcmp(existing_hdr.root_monolith.vertex_packed, ws.root_monolith.vertex_packed, ZVD_LAYER_BYTES) == 0) {
            if (out_path) std::snprintf(out_path, 512, "%s", final_path);
            if (out_deduplicated) *out_deduplicated = true;
            return true;
        }
    }

    static std::atomic<uint64_t> seq_counter{0};
    uint64_t seq = seq_counter.fetch_add(1, std::memory_order_relaxed);
    char tmp_path[576];
    std::snprintf(tmp_path, sizeof(tmp_path), "%s/.tmp.%ld.%llu.zvd3",
                  vault_dir, static_cast<long>(::getpid()), static_cast<unsigned long long>(seq));

    int fd = ::open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;

    uint32_t cube_count = static_cast<uint32_t>(ws.active_cube_count);
    uint32_t cube_off = static_cast<uint32_t>(sizeof(MacroCellDiskHeader));
    uint32_t cube_bytes = cube_count * static_cast<uint32_t>(sizeof(CubeLevel2Anchor));

    uint32_t dir_count = static_cast<uint32_t>(ws.active_micro_count);
    uint32_t dir_off = cube_off + cube_bytes;
    uint32_t dir_bytes = dir_count * static_cast<uint32_t>(sizeof(MicroTetraDirectoryEntry));
    uint32_t payload_off = dir_off + dir_bytes;

    uint32_t cur_file_off = payload_off;
    for (uint32_t i = 0; i < dir_count; ++i) {
        ws.directory[i].file_offset = cur_file_off;
        cur_file_off += (ws.directory[i].stored_len_and_flags & ZVD3_SLOT_SIZE_MASK);
    }

    MacroCellDiskHeader hdr{};
    hdr.magic = ZVD3_MAGIC;
    hdr.version = ZVD3_VERSION;
    hdr.topology_mode = static_cast<uint8_t>(ws.mode);
    hdr.raw_payload_bytes = static_cast<uint64_t>(len);
    hdr.packed_payload_bytes = static_cast<uint64_t>(ws.payload_used);
    hdr.root_hash64 = ws.root_hash64;
    hdr.cubes_hash64 = (cube_bytes > 0)
                           ? zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ws.cubes.data()), cube_bytes)
                           : 0ULL;
    hdr.directory_hash64 = (dir_bytes > 0)
                               ? zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ws.directory.data()), dir_bytes)
                               : 0ULL;
    hdr.root_gl_invariant = ws.root_gl_invariant;
    hdr.root_monolith = ws.root_monolith;
    std::memcpy(hdr.icosa_anchor_120, ws.icosa_anchor_120.data(), ZVD_ICOSA_ANCHOR_BYTES);
    hdr.active_cube_count = cube_count;
    hdr.cubes_offset = cube_off;
    hdr.active_micro_count = dir_count;
    hdr.directory_offset = dir_off;
    hdr.payload_base_offset = payload_off;
    hdr.header_hash64 = ZvdNestedCodecDetail::compute_header_integrity_hash(hdr);

    bool ok = ZvdNestedCodecDetail::write_all_fd(fd, &hdr, sizeof(hdr));
    if (ok && cube_bytes > 0) {
        ok = ZvdNestedCodecDetail::write_all_fd(fd, ws.cubes.data(), cube_bytes);
    }
    if (ok && dir_bytes > 0) {
        ok = ZvdNestedCodecDetail::write_all_fd(fd, ws.directory.data(), dir_bytes);
    }
    if (ok && ws.payload_used > 0) {
        ok = ZvdNestedCodecDetail::write_all_fd(fd, ws.payload_arena.data(), ws.payload_used);
    }
    if (ok) {
        (void)::fsync(fd);
    }
    if (::close(fd) != 0) {
        ok = false;
    }

    if (!ok || ::rename(tmp_path, final_path) != 0) {
        (void)::unlink(tmp_path);
        return false;
    }

    ZvdNestedCodecDetail::fsync_vault_dir(vault_dir);
    if (out_path) std::snprintf(out_path, 512, "%s", final_path);
    return true;
#endif
}
// =====================================================================
// SECTION 9: VECTOR SEARCH, POINTWISE pread() SLOT & FULL CELL READ
// =====================================================================

inline bool zvd_storage_search_by_vector(
    const char* vault_dir,
    const ZVD_Monolith334& query_monolith,
    int8_t query_gl_inv,
    char out_best_path[512],
    double* out_best_score,
    MacroCellDiskHeader* out_best_header = nullptr
) noexcept {
#ifdef _WIN32
    (void)vault_dir; (void)query_monolith; (void)query_gl_inv; (void)out_best_path; (void)out_best_score; (void)out_best_header;
    return false;
#else
    if (!vault_dir || !out_best_path) return false;
    DIR* dir = ::opendir(vault_dir);
    if (!dir) return false;

    double best_score = -1.0;
    bool found = false;
    char best_path[512]{0};
    MacroCellDiskHeader best_hdr{};

    struct dirent* ent = nullptr;
    while ((ent = ::readdir(dir)) != nullptr) {
        const char* name = ent->d_name;
        if (name[0] == '.') continue;
        size_t nlen = std::strlen(name);
        if (nlen < 6 || std::strcmp(name + nlen - 5, ".zvd3") != 0) continue;

        char full_path[512];
        std::snprintf(full_path, sizeof(full_path), "%s/%s", vault_dir, name);

        int fd = ::open(full_path, O_RDONLY);
        if (fd < 0) continue;

        MacroCellDiskHeader hdr{};
        bool dummy_veto = false;
        bool ok = ZvdNestedCodecDetail::read_and_verify_header(fd, hdr, &dummy_veto);
        ::close(fd);

        if (!ok || hdr.topology_mode == static_cast<uint8_t>(ZvdCodecTopologyMode::FLAT_ZVDO)) continue;

        double score = zvd_nested_resonance_score(
            hdr.root_monolith, hdr.root_gl_invariant, query_monolith, query_gl_inv
        );
        if (!found || score > best_score) {
            found = true;
            best_score = score;
            std::snprintf(best_path, sizeof(best_path), "%s", full_path);
            best_hdr = hdr;
        }
    }

    ::closedir(dir);
    if (!found) return false;

    std::snprintf(out_best_path, 512, "%s", best_path);
    if (out_best_score) *out_best_score = best_score;
    if (out_best_header) *out_best_header = best_hdr;
    return true;
#endif
}

inline bool zvd_storage_pread_slot(
    const char* zvd3_path,
    uint16_t geo_id,
    uint16_t slot_idx,
    const char* master_key,
    uint8_t* out_slot_data,
    size_t out_cap,
    size_t* out_slot_len,
    bool* out_asil_d_veto = nullptr
) noexcept {
#ifdef _WIN32
    (void)zvd3_path; (void)geo_id; (void)slot_idx; (void)master_key; (void)out_slot_data; (void)out_cap; (void)out_slot_len; (void)out_asil_d_veto;
    return false;
#else
    if (out_asil_d_veto) *out_asil_d_veto = false;
    if (!zvd3_path || slot_idx >= ZVD_MICRO_SLOTS_COUNT || !out_slot_data || out_cap == 0) {
        return false;
    }

    int fd = ::open(zvd3_path, O_RDONLY);
    if (fd < 0) return false;

    MacroCellDiskHeader hdr{};
    if (!ZvdNestedCodecDetail::read_and_verify_header(fd, hdr, out_asil_d_veto)) {
        ::close(fd);
        return false;
    }

    ZvdCodecTopologyMode mode = static_cast<ZvdCodecTopologyMode>(hdr.topology_mode);
    size_t l1 = 0, l2 = 0, l3 = 0, slot_b = 0;
    ZvdNestedCodecDetail::geometry_dims_for_mode(mode, l1, l2, l3, slot_b);
    if (geo_id >= l3 ||
        hdr.active_micro_count == 0 ||
        hdr.active_micro_count > l3 ||
        hdr.active_cube_count > l2) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    size_t cube_idx = static_cast<size_t>(geo_id) / ZVD_STELLA8_MICROS_PER_CUBE;
    if (cube_idx >= l2) cube_idx = l2 - 1u;

    CubeLevel2Anchor parent_cube{};
    if (hdr.active_cube_count > 0) {
        if (cube_idx >= static_cast<size_t>(hdr.active_cube_count)) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        size_t all_cubes_bytes = static_cast<size_t>(hdr.active_cube_count) * sizeof(CubeLevel2Anchor);
        alignas(64) std::array<CubeLevel2Anchor, ZVD_EDGE12_L2_CUBES> all_cubes{};
        if (!ZvdNestedCodecDetail::pread_all_fd(fd, all_cubes.data(), all_cubes_bytes, static_cast<off_t>(hdr.cubes_offset))) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        if (zvd_fast_hash64(reinterpret_cast<const uint8_t*>(all_cubes.data()), all_cubes_bytes) != hdr.cubes_hash64) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        parent_cube = all_cubes[cube_idx];
    } else {
        for (size_t j = 0; j < ZVD_CUBE_ANCHOR_BYTES; ++j) {
            parent_cube.data[j] = hdr.icosa_anchor_120[(cube_idx * ZVD_CUBE_ANCHOR_BYTES + j) % ZVD_ICOSA_ANCHOR_BYTES];
        }
    }

    uint32_t lo = 0;
    uint32_t hi = hdr.active_micro_count;
    bool entry_found = false;
    MicroTetraDirectoryEntry entry{};

    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) / 2u);
        MicroTetraDirectoryEntry cand{};
        off_t cand_off = static_cast<off_t>(hdr.directory_offset) +
                         static_cast<off_t>(mid) * static_cast<off_t>(sizeof(MicroTetraDirectoryEntry));
        if (!ZvdNestedCodecDetail::pread_all_fd(fd, &cand, sizeof(cand), cand_off)) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        if (cand.geo_id == geo_id) {
            entry = cand;
            entry_found = true;
            break;
        } else if (cand.geo_id < geo_id) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }

    if (!entry_found) {
        ::close(fd);
        return false;
    }

    if (entry.vertex_base5_code > ZVD3_MAX_BASE5_CODE ||
        (entry.slot_mask & static_cast<uint16_t>(1u << slot_idx)) == 0) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    uint32_t wire_len = entry.stored_len_and_flags & ZVD3_SLOT_SIZE_MASK;
    if (wire_len <= ZVD_MICRO_LEN_TABLE_BYTES ||
        wire_len > ZVD_MICRO_LEN_TABLE_BYTES + ZVD_MAX_MICRO_RAW_BYTES + 4096u) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    static thread_local alignas(64) std::array<uint8_t, ZVD_MICRO_LEN_TABLE_BYTES + ZVD_MAX_MICRO_RAW_BYTES + 4096u> micro_wire_blob{};
    bool read_ok = ZvdNestedCodecDetail::pread_all_fd(
        fd, micro_wire_blob.data(), wire_len, static_cast<off_t>(entry.file_offset)
    );
    ::close(fd);

    if (!read_ok) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    uint8_t slot_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES]{};
    uint16_t slot_lens[ZVD_MICRO_SLOTS_COUNT]{0};

    bool unpack_ok = ZvdNestedCodecDetail::unpack_single_micro_tetra(
        entry.geo_id,
        entry.vertex_base5_code,
        entry.slot_mask,
        entry.raw_micro_bytes,
        entry.stored_len_and_flags,
        entry.coupled_micro_tag32,
        hdr.icosa_anchor_120,
        parent_cube,
        micro_wire_blob.data(),
        wire_len,
        master_key,
        mode,
        slot_slices,
        slot_lens
    );

    if (!unpack_ok) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    uint16_t target_len = slot_lens[slot_idx];
    if (target_len == 0 || static_cast<size_t>(target_len) > out_cap || static_cast<size_t>(target_len) > slot_b) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    std::memcpy(out_slot_data, slot_slices[slot_idx], target_len);
    if (out_slot_len) *out_slot_len = static_cast<size_t>(target_len);
    return true;
#endif
}

inline bool zvd_storage_get_full_cell(
    const char* zvd3_path,
    const char* master_key,
    uint8_t* out_data,
    size_t out_cap,
    size_t* out_len,
    bool* out_asil_d_veto = nullptr
) noexcept {
#ifdef _WIN32
    (void)zvd3_path; (void)master_key; (void)out_data; (void)out_cap; (void)out_len; (void)out_asil_d_veto;
    return false;
#else
    if (out_asil_d_veto) *out_asil_d_veto = false;
    if (!zvd3_path || !out_data || !out_len) return false;

    int fd = ::open(zvd3_path, O_RDONLY);
    if (fd < 0) return false;

    MacroCellDiskHeader hdr{};
    if (!ZvdNestedCodecDetail::read_and_verify_header(fd, hdr, out_asil_d_veto)) {
        ::close(fd);
        return false;
    }

    if (hdr.topology_mode == static_cast<uint8_t>(ZvdCodecTopologyMode::FLAT_ZVDO)) {
        size_t wire_sz = static_cast<size_t>(hdr.packed_payload_bytes);
        if (wire_sz == 0 || wire_sz > ZVD_OPTIC_MAX_BLOCK_BYTES + 2048 ||
            static_cast<size_t>(hdr.raw_payload_bytes) > out_cap) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        alignas(64) uint8_t optic_wire[ZVD_OPTIC_MAX_BLOCK_BYTES + 2048];
        if (!ZvdNestedCodecDetail::pread_all_fd(fd, optic_wire, wire_sz, static_cast<off_t>(hdr.payload_base_offset))) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        ::close(fd);
        if (zvd_fast_hash64(optic_wire, wire_sz) != hdr.directory_hash64) {
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        const char* k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
        uint16_t sr = 0, dr = 0;
        size_t dec_sz = 0;
        bool bp = false;
        if (!zvd_optic_frame_unpack(optic_wire, wire_sz, k, out_data, out_cap, &sr, &dr, &dec_sz, &bp) || !bp) {
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
        *out_len = dec_sz;
        return true;
    }

    if (static_cast<size_t>(hdr.raw_payload_bytes) > out_cap ||
        static_cast<size_t>(hdr.packed_payload_bytes) > sizeof(MacroCellWorkspace::payload_arena) ||
        hdr.active_micro_count > ZVD_EDGE12_L3_MICROS ||
        hdr.active_cube_count > ZVD_EDGE12_L2_CUBES) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    static thread_local alignas(64) MacroCellWorkspace ws;
    ZvdCodecTopologyMode mode = static_cast<ZvdCodecTopologyMode>(hdr.topology_mode);
    ZvdNestedCodecDetail::geometry_dims_for_mode(mode, ws.l1_tetras, ws.l2_cubes, ws.l3_micros, ws.slot_bytes);
    ws.mode = mode;
    ws.active_cube_count = static_cast<size_t>(hdr.active_cube_count);
    ws.active_micro_count = static_cast<size_t>(hdr.active_micro_count);
    ws.raw_payload_bytes = static_cast<size_t>(hdr.raw_payload_bytes);
    ws.root_hash64 = hdr.root_hash64;
    ws.root_gl_invariant = hdr.root_gl_invariant;
    ws.root_monolith = hdr.root_monolith;
    std::memcpy(ws.icosa_anchor_120.data(), hdr.icosa_anchor_120, ZVD_ICOSA_ANCHOR_BYTES);
    ws.payload_used = static_cast<size_t>(hdr.packed_payload_bytes);

    size_t cube_bytes = ws.active_cube_count * sizeof(CubeLevel2Anchor);
    size_t dir_bytes = ws.active_micro_count * sizeof(MicroTetraDirectoryEntry);
    bool ok = true;

    if (cube_bytes > 0) {
        ok = ZvdNestedCodecDetail::pread_all_fd(fd, ws.cubes.data(), cube_bytes, static_cast<off_t>(hdr.cubes_offset));
        if (ok && zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ws.cubes.data()), cube_bytes) != hdr.cubes_hash64) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
    }

    if (ok && dir_bytes > 0) {
        ok = ZvdNestedCodecDetail::pread_all_fd(fd, ws.directory.data(), dir_bytes, static_cast<off_t>(hdr.directory_offset));
        if (ok && zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ws.directory.data()), dir_bytes) != hdr.directory_hash64) {
            ::close(fd);
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
    }

    if (ok && ws.payload_used > 0) {
        ok = ZvdNestedCodecDetail::pread_all_fd(fd, ws.payload_arena.data(), ws.payload_used, static_cast<off_t>(hdr.payload_base_offset));
    }
    ::close(fd);

    if (!ok) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    bool bit_perfect = false;
    size_t decoded_sz = 0;
    if (!zvd_nested_decode_macro_cell(ws, master_key, out_data, out_cap, &decoded_sz, &bit_perfect) || !bit_perfect) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    *out_len = decoded_sz;
    return true;
#endif
}
// =====================================================================
// SECTION 10: CONSTRUCTOR BUS REGISTRATION (TELEMETRY CARTRIDGE)
// =====================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_nested_storage_codec_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_NESTED_MODE_EDGE12")) {
        zvd_set_codec_mode(ZvdCodecTopologyMode::EDGE12_TETRA);
        return 1;
    }
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_NESTED_MODE_STELLA8")) {
        zvd_set_codec_mode(ZvdCodecTopologyMode::STELLA_8_VERT);
        return 1;
    }
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_NESTED_MODE_FLAT")) {
        zvd_set_codec_mode(ZvdCodecTopologyMode::FLAT_ZVDO);
        return 1;
    }
    return 0;
}

inline bool zvd_nested_storage_codec_verify(std::string_view prompt, std::string_view response) noexcept {
    (void)prompt;
    (void)response;
    return true;
}

ZVD_REGISTER_CARTRIDGE_CTX(
    "ZVD-Nested-Storage-Codec-v32",
    ZvdModuleRole::TELEMETRY,
    zvd_nested_storage_codec_evaluate_ctx,
    zvd_nested_storage_codec_verify
);
#endif
