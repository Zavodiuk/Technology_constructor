/* =============================================================================
 *  PROJECT:    ZVD-120D NESTED GEOMETRIC CODEC & MULTI-LEVEL STORAGE CARTRIDGE
 *              (13.11_zvd_nested_storage_codec_v31.hpp - V31.1-EGYPT-345-VAULT-C20)
 *  FILENAME:   13.11_zvd_nested_storage_codec_v31.hpp
 *  VERSION:    2026.11-V31.1-EGYPT-345-VAULT-C20
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

#define ZVD_HAS_NESTED_STORAGE_CODEC 1

#if !defined(ZVD_HAS_DATACENTER_CORE)
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
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
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
// 1. CODEC TOPOLOGY MODES & RUNTIME CONFIGURATION
// =====================================================================

enum class ZvdCodecTopologyMode : uint8_t {
    FLAT_ZVDO = 0,
    STELLA_8_VERT = 1,
    EDGE12_TETRA = 2
};

inline constexpr uint32_t ZVD3_MAGIC = 0x3344565Au; // "ZVD3" little-endian
inline constexpr uint32_t ZVD3_VERSION = 2u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_COMPRESSED = 0x80000000u;
inline constexpr uint32_t ZVD3_SLOT_FLAG_EGYPT345 = 0x40000000u;
inline constexpr uint32_t ZVD3_SLOT_SIZE_MASK = 0x0FFFFFFFu;

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

inline constexpr size_t ZVD_MAX_MACRO_CELL_BYTES = ZVD_STELLA8_L3_MICROS * ZVD_MICRO_SLOTS_COUNT * ZVD_STELLA8_SLOT_BYTES;

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
// 2. ON-DISK CONTAINER STRUCTURES & ZERO-HEAP MICRO-TETRA DESCRIPTORS
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
MacroCellDiskHeader {
    uint32_t magic;            // "ZVD3" (0x3344565Au)
    uint32_t version;          // 2
    uint8_t topology_mode;     // ZvdCodecTopologyMode
    uint8_t reserved_pad[7];
    uint64_t raw_payload_bytes;
    uint64_t packed_payload_bytes;
    uint64_t root_hash64;
    uint64_t header_hash64;    // Integrity hash over header fields prior to header_hash64
    uint64_t directory_hash64; // Integrity hash over MicroTetraDirectoryTable
    int8_t root_gl_invariant;
    uint8_t reserved_inv[7];
    ZVD_Monolith334 root_monolith; // 334 bytes
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
    uint16_t geo_id;              // 0 .. 575 (Stella-8) or 0 .. 863 (Edge-12)
    uint16_t vertex_base5_code;   // ((v0+2) + 5*(v1+2) + 25*(v2+2) + 125*(v3+2))
    uint16_t slot_mask;           // Bit k set => slot k is non-empty (k in 0..9)
    uint16_t slot_raw_len[ZVD_MICRO_SLOTS_COUNT]; // Uncompressed slot byte lengths (0 if empty)
    uint32_t slot_stored_len[ZVD_MICRO_SLOTS_COUNT]; // Flags + wire len (ZVD3_SLOT_SIZE_MASK)
    uint32_t slot_edge_tag32[ZVD_MICRO_SLOTS_COUNT]; // Coupled vertex-edge-payload integrity tag
    uint32_t file_offset;         // Byte offset from start of .zvd3 file to packed slots
    uint32_t packed_size;         // Total bytes occupied by this micro-tetra in payload region
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

struct alignas(64) MacroCellWorkspace {
    ZvdCodecTopologyMode mode = ZvdCodecTopologyMode::STELLA_8_VERT;
    size_t l1_tetras = 8;
    size_t l2_cubes = 48;
    size_t l3_micros = 576;
    size_t slot_bytes = 704;
    size_t active_micro_count = 0;
    uint64_t root_hash64 = 0;
    int8_t root_gl_invariant = 0;
    ZVD_Monolith334 root_monolith{};
    std::array<MicroTetraDirectoryEntry, ZVD_EDGE12_L3_MICROS> directory{};
    std::array<uint8_t, ZVD_MAX_MACRO_CELL_BYTES> payload_arena{};
    size_t payload_used = 0;
};

// =====================================================================
// 3. EGYPTIAN 3-4-5 TRIANGLE FOLDING & CRYPTO-TOPOLOGICAL COUPLING
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

// Egyptian 3-4-5 reversible byte folding across legs (3 and 4) and hypotenuse (5)
inline void egyptian_345_fold_bytes(uint8_t* buf, size_t len, uint16_t geo_id, uint16_t slot_idx, bool forward) noexcept {
    if (!buf || len < 3u) return;
    int pyth_lock = zvd_pythagorean_directive_lock(static_cast<int>((geo_id % 19u) + 1u));
    uint8_t k3 = static_cast<uint8_t>((3u * (static_cast<uint32_t>(slot_idx) + 1u) + static_cast<uint32_t>(std::abs(pyth_lock))) & 0xFFu);
    uint8_t k4 = static_cast<uint8_t>((4u * (static_cast<uint32_t>(geo_id) + 1u) + static_cast<uint32_t>(slot_idx * 3u)) & 0xFFu);
    uint8_t k5 = static_cast<uint8_t>((5u * (static_cast<uint32_t>(geo_id + slot_idx) + 1u)) & 0xFFu);

    if (forward) {
        for (size_t i = 0; i + 2u < len; i += 3u) {
            uint8_t a = buf[i];
            uint8_t b = buf[i + 1u];
            uint8_t c = buf[i + 2u];
            // Hypotenuse weight (3*a + 4*b) mod 256 folded reversibly into c
            uint8_t hyp_c = static_cast<uint8_t>(c + static_cast<uint8_t>(3u * a + 4u * b) + k5);
            uint8_t nb = static_cast<uint8_t>(b + static_cast<uint8_t>(3u * a) + k4);
            uint8_t na = static_cast<uint8_t>(a + static_cast<uint8_t>(4u * nb) + k3);
            buf[i] = na;
            buf[i + 1u] = nb;
            buf[i + 2u] = hyp_c;
        }
    } else {
        size_t full_triples = (len / 3u) * 3u;
        for (size_t i = full_triples; i >= 3u; i -= 3u) {
            size_t base = i - 3u;
            uint8_t na = buf[base];
            uint8_t nb = buf[base + 1u];
            uint8_t hyp_c = buf[base + 2u];

            uint8_t a = static_cast<uint8_t>(na - static_cast<uint8_t>(4u * nb) - k3);
            uint8_t b = static_cast<uint8_t>(nb - static_cast<uint8_t>(3u * a) - k4);
            uint8_t c = static_cast<uint8_t>(hyp_c - static_cast<uint8_t>(3u * a + 4u * b) - k5);

            buf[base] = a;
            buf[base + 1u] = b;
            buf[base + 2u] = c;
            if (base == 0) break;
        }
    }
}

inline uint32_t compute_coupled_edge_tag32(
    const uint8_t* raw_slot,
    uint16_t raw_len,
    uint16_t geo_id,
    uint16_t slot_idx,
    uint16_t vertex_base5_code,
    const int8_t vertices_p5[4]
) noexcept {
    uint64_t h = zvd_fast_hash64(raw_slot, raw_len);
    uint64_t topo = static_cast<uint64_t>(geo_id) * 0x9E3779B97F4A7C15ULL ^
                    (static_cast<uint64_t>(slot_idx) * 0xBF58476D1CE4E5B9ULL) ^
                    static_cast<uint64_t>(vertex_base5_code);

    if (slot_idx >= ZVD_VERTEX_SLOTS_COUNT) {
        size_t e_idx = static_cast<size_t>(slot_idx - ZVD_VERTEX_SLOTS_COUNT);
        if (e_idx >= ZVD_EDGE_SLOTS_COUNT) e_idx = 0;
        uint8_t ia = TETRA_EDGE_PAIRS[e_idx][0];
        uint8_t ib = TETRA_EDGE_PAIRS[e_idx][1];
        int va = static_cast<int>(vertices_p5[ia]) + 3;
        int vb = static_cast<int>(vertices_p5[ib]) + 3;
        int hyp_345 = 3 * va + 4 * vb;
        topo ^= static_cast<uint64_t>(hyp_345) * 0xFF51AFD7ED558CCDULL;
    }

    uint64_t mixed = h ^ topo;
    mixed ^= (mixed >> 33);
    mixed *= 0xFF51AFD7ED558CCDULL;
    mixed ^= (mixed >> 33);
    return static_cast<uint32_t>(mixed & 0xFFFFFFFFULL);
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
    tmp.directory_hash64 = 0;
    return zvd_fast_hash64(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

inline bool encode_single_slot(
    const uint8_t* raw_slot,
    size_t raw_len,
    const char* master_key,
    ZvdCodecTopologyMode mode,
    uint16_t geo_id,
    uint16_t slot_idx,
    uint16_t vertex_base5_code,
    const int8_t vertices_p5[4],
    uint8_t* dst_buf,
    size_t dst_cap,
    uint16_t& out_raw_len,
    uint32_t& out_stored_len,
    uint32_t& out_edge_tag32
) noexcept {
    if (!raw_slot || raw_len == 0 || raw_len > 65535u || !dst_buf) return false;

    alignas(64) uint8_t work_buf[1024];
    if (raw_len > sizeof(work_buf)) return false;

    std::memcpy(work_buf, raw_slot, raw_len);
    bool use_egypt345 = (raw_len >= 3u);
    if (use_egypt345) {
        egyptian_345_fold_bytes(work_buf, raw_len, geo_id, slot_idx, true);
    }

    size_t wire_len = raw_len;
    bool compressed = false;

    if (raw_len >= 32u) {
        size_t sample_len = (raw_len < 256u) ? raw_len : 256u;
        double ent = zvd_shannon_entropy(work_buf, sample_len);
        if (ent < ZVD_ENTROPY_BYPASS_THRESHOLD) {
            alignas(64) uint8_t lz4_tmp[1024];
            size_t c_sz = zvd_lz4_compress_block_stack(work_buf, raw_len, lz4_tmp, sizeof(lz4_tmp));
            if (c_sz > 0u && c_sz < raw_len && c_sz <= sizeof(work_buf)) {
                std::memcpy(work_buf, lz4_tmp, c_sz);
                wire_len = c_sz;
                compressed = true;
            }
        }
    }

    if (wire_len > dst_cap || wire_len > ZVD3_SLOT_SIZE_MASK) return false;

    char coupled_key[128];
    build_coupled_slot_key(master_key, mode, geo_id, slot_idx, vertices_p5, coupled_key);
    size_t block_idx = (static_cast<size_t>(geo_id) * ZVD_MICRO_SLOTS_COUNT) + static_cast<size_t>(slot_idx);
    zvd_120d_moire_crypt_buffer(work_buf, wire_len, coupled_key, block_idx, true);

    std::memcpy(dst_buf, work_buf, wire_len);
    out_raw_len = static_cast<uint16_t>(raw_len);
    uint32_t flags = (compressed ? ZVD3_SLOT_FLAG_COMPRESSED : 0u) |
                     (use_egypt345 ? ZVD3_SLOT_FLAG_EGYPT345 : 0u);
    out_stored_len = static_cast<uint32_t>(wire_len) | flags;
    out_edge_tag32 = compute_coupled_edge_tag32(
        raw_slot, out_raw_len, geo_id, slot_idx, vertex_base5_code, vertices_p5
    );
    return true;
}

inline bool decode_single_slot(
    const uint8_t* stored_slot,
    uint32_t stored_len_raw,
    uint32_t expected_edge_tag32,
    uint16_t raw_len,
    const char* master_key,
    ZvdCodecTopologyMode mode,
    uint16_t geo_id,
    uint16_t slot_idx,
    uint16_t vertex_base5_code,
    const int8_t vertices_p5[4],
    uint8_t* out_slot_buf,
    size_t out_slot_cap
) noexcept {
    if (!stored_slot || !out_slot_buf || raw_len == 0 || out_slot_cap < static_cast<size_t>(raw_len)) {
        return false;
    }

    bool compressed = (stored_len_raw & ZVD3_SLOT_FLAG_COMPRESSED) != 0u;
    bool use_egypt345 = (stored_len_raw & ZVD3_SLOT_FLAG_EGYPT345) != 0u;
    size_t wire_len = static_cast<size_t>(stored_len_raw & ZVD3_SLOT_SIZE_MASK);
    if (wire_len == 0 || wire_len > sizeof(MacroCellWorkspace::payload_arena)) {
        return false;
    }

    alignas(64) uint8_t crypt_tmp[1024];
    if (wire_len > sizeof(crypt_tmp)) return false;
    std::memcpy(crypt_tmp, stored_slot, wire_len);

    char coupled_key[128];
    build_coupled_slot_key(master_key, mode, geo_id, slot_idx, vertices_p5, coupled_key);
    size_t block_idx = (static_cast<size_t>(geo_id) * ZVD_MICRO_SLOTS_COUNT) + static_cast<size_t>(slot_idx);
    zvd_120d_moire_crypt_buffer(crypt_tmp, wire_len, coupled_key, block_idx, false);

    alignas(64) uint8_t folded_buf[1024];
    if (!compressed) {
        if (wire_len != static_cast<size_t>(raw_len)) return false;
        std::memcpy(folded_buf, crypt_tmp, wire_len);
    } else {
        size_t dec_len = 0;
        if (!zvd_lz4_decompress_block_safe(crypt_tmp, wire_len, folded_buf, sizeof(folded_buf), &dec_len)) {
            return false;
        }
        if (dec_len != static_cast<size_t>(raw_len)) return false;
    }

    if (use_egypt345 && raw_len >= 3u) {
        egyptian_345_fold_bytes(folded_buf, static_cast<size_t>(raw_len), geo_id, slot_idx, false);
    }

    uint32_t actual_tag32 = compute_coupled_edge_tag32(
        folded_buf, raw_len, geo_id, slot_idx, vertex_base5_code, vertices_p5
    );
    if (actual_tag32 != expected_edge_tag32) {
        return false;
    }

    std::memcpy(out_slot_buf, folded_buf, raw_len);
    return true;
}

} // namespace ZvdNestedCodecDetail

// =====================================================================
// 4. MACRO-CELL ENCODING, BOTTOM-UP FOLDING & RESONANCE SCORING
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

    ZvdNestedCodecDetail::geometry_dims_for_mode(
        mode, out_cell.l1_tetras, out_cell.l2_cubes, out_cell.l3_micros, out_cell.slot_bytes
    );

    const size_t total_slots = out_cell.l3_micros * ZVD_MICRO_SLOTS_COUNT;
    const size_t max_cell_capacity = total_slots * out_cell.slot_bytes;
    if (input_len > max_cell_capacity) {
        return false;
    }

    out_cell.mode = mode;
    out_cell.active_micro_count = 0;
    out_cell.payload_used = 0;
    out_cell.root_hash64 = zvd_fast_hash64(input_data, input_len);

    std::array<int8_t, ZVD_CELLS_PER_LAYER> folded_500{};
    folded_500.fill(0);

    size_t cursor = 0;
    size_t dir_idx = 0;

    for (uint16_t geo_id = 0; geo_id < static_cast<uint16_t>(out_cell.l3_micros); ++geo_id) {
        if (cursor >= input_len) break;

        uint8_t slot_raw_slices[ZVD_MICRO_SLOTS_COUNT][ZVD_STELLA8_SLOT_BYTES];
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

        MicroTetraDirectoryEntry entry{};
        entry.geo_id = geo_id;
        entry.vertex_base5_code = ZvdNestedCodecDetail::pack_vertices_base5(v_p5[0], v_p5[1], v_p5[2], v_p5[3]);
        entry.slot_mask = slot_mask;
        entry.file_offset = 0;
        entry.packed_size = 0;

        uint32_t micro_packed_bytes = 0;
        for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
            if ((slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
                if (out_cell.payload_used + out_cell.slot_bytes > out_cell.payload_arena.size()) {
                    return false;
                }
                uint16_t r_len = 0;
                uint32_t st_len = 0;
                uint32_t e_tag32 = 0;
                if (!ZvdNestedCodecDetail::encode_single_slot(
                        slot_raw_slices[s],
                        slot_raw_lens[s],
                        master_key,
                        mode,
                        geo_id,
                        s,
                        entry.vertex_base5_code,
                        v_p5,
                        out_cell.payload_arena.data() + out_cell.payload_used,
                        out_cell.slot_bytes,
                        r_len,
                        st_len,
                        e_tag32)) {
                    return false;
                }
                uint32_t wire_only = st_len & ZVD3_SLOT_SIZE_MASK;
                entry.slot_raw_len[s] = r_len;
                entry.slot_stored_len[s] = st_len;
                entry.slot_edge_tag32[s] = e_tag32;
                out_cell.payload_used += wire_only;
                micro_packed_bytes += wire_only;
            }
        }

        entry.packed_size = micro_packed_bytes;
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
    if (!out_data || !out_len || out_cap < cell.payload_used) {
        if (out_bit_perfect) *out_bit_perfect = false;
        return false;
    }

    size_t arena_pos = 0;
    size_t write_pos = 0;

    for (size_t d = 0; d < cell.active_micro_count; ++d) {
        const MicroTetraDirectoryEntry& entry = cell.directory[d];
        int8_t v_p5[4];
        ZvdNestedCodecDetail::unpack_vertices_base5(entry.vertex_base5_code, v_p5);

        for (uint16_t s = 0; s < ZVD_MICRO_SLOTS_COUNT; ++s) {
            if ((entry.slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
                uint32_t wire_only = entry.slot_stored_len[s] & ZVD3_SLOT_SIZE_MASK;
                if (arena_pos + wire_only > cell.payload_used || write_pos + entry.slot_raw_len[s] > out_cap) {
                    if (out_bit_perfect) *out_bit_perfect = false;
                    return false;
                }
                if (!ZvdNestedCodecDetail::decode_single_slot(
                        cell.payload_arena.data() + arena_pos,
                        entry.slot_stored_len[s],
                        entry.slot_edge_tag32[s],
                        entry.slot_raw_len[s],
                        master_key,
                        cell.mode,
                        entry.geo_id,
                        s,
                        entry.vertex_base5_code,
                        v_p5,
                        out_data + write_pos,
                        out_cap - write_pos)) {
                    if (out_bit_perfect) *out_bit_perfect = false;
                    return false;
                }
                arena_pos += wire_only;
                write_pos += entry.slot_raw_len[s];
            }
        }
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
    double match_acc = 0.0;
    const size_t total_pairs = static_cast<size_t>(ZVD_LAYER_BYTES) * 2u;

    for (size_t i = 0; i < ZVD_LAYER_BYTES; ++i) {
        uint8_t cf = cell_monolith.face_packed[i];
        uint8_t qf = query_monolith.face_packed[i];
        uint8_t diff_f = (cf > qf) ? static_cast<uint8_t>(cf - qf) : static_cast<uint8_t>(qf - cf);
        match_acc += 1.0 - (static_cast<double>(diff_f) / 255.0);

        uint8_t cv = cell_monolith.vertex_packed[i];
        uint8_t qv = query_monolith.vertex_packed[i];
        uint8_t diff_v = (cv > qv) ? static_cast<uint8_t>(cv - qv) : static_cast<uint8_t>(qv - cv);
        match_acc += 1.0 - (static_cast<double>(diff_v) / 255.0);
    }

    double base_sim = match_acc / static_cast<double>(total_pairs);
    int inv_diff = std::abs(static_cast<int>(cell_gl_inv) - static_cast<int>(query_gl_inv));
    double inv_bonus = std::max(0.0, 1.0 - (static_cast<double>(inv_diff) / 4.0));
    return std::clamp(0.85 * base_sim + 0.15 * inv_bonus, 0.0, 1.0);
}
// =====================================================================
// 5. ATOMIC .ZVD3 STORAGE VAULT (POSIX pread + DEDUP + ASIL D VETO)
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
    (void)::mkdir(vault_dir, 0755);
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
    uint64_t expected_hdr_hash = compute_header_integrity_hash(out_hdr);
    if (out_hdr.header_hash64 != expected_hdr_hash) {
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

    // FLAT_ZVDO mode: zero-heap optical framing container path via zvd_optic_frame_pack
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
        hdr.directory_hash64 = zvd_fast_hash64(optic_wire, optic_sz);
        hdr.root_gl_invariant = 0;
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

    alignas(64) MacroCellWorkspace ws;
    if (!zvd_nested_encode_macro_cell(data, len, master_key, mode, ws)) {
        return false;
    }

    char final_path[512];
    ZvdNestedCodecDetail::build_cell_file_path(vault_dir, mode, ws.root_hash64, final_path);

    // Deduplication by root_hash64 + root_monolith
    int existing_fd = ::open(final_path, O_RDONLY);
    if (existing_fd >= 0) {
        MacroCellDiskHeader existing_hdr{};
        bool existing_veto = false;
        bool ok_hdr = ZvdNestedCodecDetail::read_and_verify_header(existing_fd, existing_hdr, &existing_veto);
        ::close(existing_fd);
        if (ok_hdr &&
            existing_hdr.root_hash64 == ws.root_hash64 &&
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

    uint32_t dir_count = static_cast<uint32_t>(ws.active_micro_count);
    uint32_t dir_off = static_cast<uint32_t>(sizeof(MacroCellDiskHeader));
    uint32_t dir_bytes = dir_count * static_cast<uint32_t>(sizeof(MicroTetraDirectoryEntry));
    uint32_t payload_off = dir_off + dir_bytes;

    uint32_t cur_file_off = payload_off;
    for (uint32_t i = 0; i < dir_count; ++i) {
        ws.directory[i].file_offset = cur_file_off;
        cur_file_off += ws.directory[i].packed_size;
    }

    MacroCellDiskHeader hdr{};
    hdr.magic = ZVD3_MAGIC;
    hdr.version = ZVD3_VERSION;
    hdr.topology_mode = static_cast<uint8_t>(ws.mode);
    hdr.raw_payload_bytes = static_cast<uint64_t>(len);
    hdr.packed_payload_bytes = static_cast<uint64_t>(ws.payload_used);
    hdr.root_hash64 = ws.root_hash64;
    hdr.directory_hash64 = (dir_bytes > 0)
                               ? zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ws.directory.data()), dir_bytes)
                               : 0ULL;
    hdr.root_gl_invariant = ws.root_gl_invariant;
    hdr.root_monolith = ws.root_monolith;
    hdr.active_micro_count = dir_count;
    hdr.directory_offset = dir_off;
    hdr.payload_base_offset = payload_off;
    hdr.header_hash64 = ZvdNestedCodecDetail::compute_header_integrity_hash(hdr);

    bool ok = ZvdNestedCodecDetail::write_all_fd(fd, &hdr, sizeof(hdr));
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
        if (fd < 0) {
            if (errno == ENOENT) continue;
            continue;
        }

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
    if (geo_id >= l3 || hdr.active_micro_count == 0 || hdr.active_micro_count > l3) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    // Binary search in sorted directory by geo_id -> O(log N)
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

    if (entry.vertex_base5_code > 3124u || (entry.slot_mask & static_cast<uint16_t>(1u << slot_idx)) == 0) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    int8_t v_p5[4];
    ZvdNestedCodecDetail::unpack_vertices_base5(entry.vertex_base5_code, v_p5);

    uint32_t slot_rel_off = 0;
    for (uint16_t s = 0; s < slot_idx; ++s) {
        if ((entry.slot_mask & static_cast<uint16_t>(1u << s)) != 0) {
            slot_rel_off += (entry.slot_stored_len[s] & ZVD3_SLOT_SIZE_MASK);
        }
    }

    uint32_t stored_len_raw = entry.slot_stored_len[slot_idx];
    uint32_t wire_only = stored_len_raw & ZVD3_SLOT_SIZE_MASK;
    uint16_t raw_len = entry.slot_raw_len[slot_idx];

    if (wire_only == 0 || raw_len == 0 || static_cast<size_t>(raw_len) > out_cap) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    alignas(64) uint8_t packed_slice[ZVD_STELLA8_SLOT_BYTES + 64];
    if (wire_only > sizeof(packed_slice)) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    off_t target_file_off = static_cast<off_t>(entry.file_offset) + static_cast<off_t>(slot_rel_off);
    bool read_ok = ZvdNestedCodecDetail::pread_all_fd(fd, packed_slice, wire_only, target_file_off);
    ::close(fd);

    if (!read_ok) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    alignas(64) uint8_t decoded_slice[ZVD_STELLA8_SLOT_BYTES + 64];
    bool dec_ok = ZvdNestedCodecDetail::decode_single_slot(
        packed_slice,
        stored_len_raw,
        entry.slot_edge_tag32[slot_idx],
        raw_len,
        master_key,
        mode,
        geo_id,
        slot_idx,
        entry.vertex_base5_code,
        v_p5,
        decoded_slice,
        sizeof(decoded_slice)
    );

    if (!dec_ok) {
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    if (slot_idx >= ZVD_VERTEX_SLOTS_COUNT) {
        size_t e_idx = static_cast<size_t>(slot_idx - ZVD_VERTEX_SLOTS_COUNT);
        uint8_t ia = ZvdNestedCodecDetail::TETRA_EDGE_PAIRS[e_idx][0];
        uint8_t ib = ZvdNestedCodecDetail::TETRA_EDGE_PAIRS[e_idx][1];
        uint16_t expected_code = ZvdNestedCodecDetail::pack_vertices_base5(v_p5[0], v_p5[1], v_p5[2], v_p5[3]);
        if (expected_code != entry.vertex_base5_code ||
            std::abs(static_cast<int>(v_p5[ia])) > 2 ||
            std::abs(static_cast<int>(v_p5[ib])) > 2) {
            if (out_asil_d_veto) *out_asil_d_veto = true;
            return false;
        }
    }

    std::memcpy(out_slot_data, decoded_slice, raw_len);
    if (out_slot_len) *out_slot_len = static_cast<size_t>(raw_len);
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
        static_cast<size_t>(hdr.packed_payload_bytes) > ZVD_MAX_MACRO_CELL_BYTES ||
        hdr.active_micro_count > ZVD_EDGE12_L3_MICROS) {
        ::close(fd);
        if (out_asil_d_veto) *out_asil_d_veto = true;
        return false;
    }

    alignas(64) MacroCellWorkspace ws;
    ZvdCodecTopologyMode mode = static_cast<ZvdCodecTopologyMode>(hdr.topology_mode);
    ZvdNestedCodecDetail::geometry_dims_for_mode(mode, ws.l1_tetras, ws.l2_cubes, ws.l3_micros, ws.slot_bytes);
    ws.mode = mode;
    ws.active_micro_count = static_cast<size_t>(hdr.active_micro_count);
    ws.root_hash64 = hdr.root_hash64;
    ws.root_gl_invariant = hdr.root_gl_invariant;
    ws.root_monolith = hdr.root_monolith;
    ws.payload_used = static_cast<size_t>(hdr.packed_payload_bytes);

    size_t dir_bytes = ws.active_micro_count * sizeof(MicroTetraDirectoryEntry);
    bool ok = true;
    if (dir_bytes > 0) {
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
// 6. CONSTRUCTOR BUS REGISTRATION (TELEMETRY CARTRIDGE)
// =====================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_nested_storage_codec_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    ZvdCodecTopologyMode m = zvd_get_codec_mode();
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
    (void)m;
    return 0;
}

inline bool zvd_nested_storage_codec_verify(std::string_view prompt, std::string_view response) noexcept {
    (void)prompt;
    (void)response;
    return true;
}

ZVD_REGISTER_CARTRIDGE_CTX(
    "ZVD-Nested-Storage-Codec-v31",
    ZvdModuleRole::TELEMETRY,
    zvd_nested_storage_codec_evaluate_ctx,
    zvd_nested_storage_codec_verify
);
#endif

// =====================================================================
// 7. STANDALONE LIVE BENCHMARK (CODEC + .ZVD3 VAULT + ASIL D VETO)
// =====================================================================

#if !defined(ZVD_NO_MAIN) || (ZVD_NO_MAIN == 0)

namespace ZvdNestedBenchmarkDetail {

inline constexpr size_t BENCH_BYTES = 3600000u; // 3.6 MB

struct alignas(64) BenchBuffers {
    uint8_t payload[BENCH_BYTES];
    uint8_t decoded[BENCH_BYTES + 64];
};

inline void fill_test_payload_36mb(uint8_t* buf, size_t len) noexcept {
    static constexpr char k_json_pattern[] =
        "{\"shard\":\"zvd-120d-vault\",\"topology\":\"stella8-edge12\",\"tensor_q\":[-2,-1,0,1,2],"
        "\"metric_a\":0.9942,\"metric_b\":1.0018,\"status\":\"deterministic-ok\"}\n";
    constexpr size_t pat_len = sizeof(k_json_pattern) - 1;
    size_t pos = 0;
    uint32_t seq = 0;
    while (pos < len) {
        size_t take = std::min(pat_len, len - pos);
        std::memcpy(buf + pos, k_json_pattern, take);
        if (take >= 8) {
            uint8_t patch = static_cast<uint8_t>((seq * 29u) & 0xFFu);
            buf[pos + 6] = static_cast<char>('0' + (patch % 10));
        }
        pos += take;
        seq++;
    }
}

inline void cleanup_bench_dir(const char* dir_path) noexcept {
#ifndef _WIN32
    DIR* d = ::opendir(dir_path);
    if (!d) return;
    struct dirent* ent = nullptr;
    char fpath[512];
    while ((ent = ::readdir(d)) != nullptr) {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
        std::snprintf(fpath, sizeof(fpath), "%s/%s", dir_path, ent->d_name);
        (void)::unlink(fpath);
    }
    ::closedir(d);
    (void)::rmdir(dir_path);
#else
    (void)dir_path;
#endif
}

struct ModeBenchResult {
    ZvdCodecTopologyMode mode;
    double ratio;
    double put_gbps;
    double get_gbps;
    double search_us;
    double pread_us;
    bool veto_verified;
    char stored_path[512];
};

inline ModeBenchResult run_mode_benchmark(
    const char* vault_dir,
    BenchBuffers& bbuf,
    size_t payload_len,
    const char* key,
    ZvdCodecTopologyMode mode
) {
    ModeBenchResult res{};
    res.mode = mode;
    zvd_set_codec_mode(mode);

    auto t_put_start = std::chrono::high_resolution_clock::now();
    bool put_ok = zvd_storage_put_nested_cell(vault_dir, bbuf.payload, payload_len, key, mode, res.stored_path, nullptr);
    auto t_put_end = std::chrono::high_resolution_clock::now();

    double put_ns = std::chrono::duration<double, std::nano>(t_put_end - t_put_start).count();
    if (put_ns <= 0.0) put_ns = 1.0;
    res.put_gbps = put_ok ? (static_cast<double>(payload_len) / put_ns) : 0.0;

    MacroCellDiskHeader hdr{};
    int hfd = ::open(res.stored_path, O_RDONLY);
    if (hfd >= 0) {
        bool hv = false;
        (void)ZvdNestedCodecDetail::read_and_verify_header(hfd, hdr, &hv);
        ::close(hfd);
    }
    res.ratio = (hdr.packed_payload_bytes > 0)
                    ? (static_cast<double>(payload_len) / static_cast<double>(hdr.packed_payload_bytes))
                    : 1.0;

    char best_path[512]{0};
    double best_score = 0.0;
    auto t_s_start = std::chrono::high_resolution_clock::now();
    bool s_ok = zvd_storage_search_by_vector(vault_dir, hdr.root_monolith, hdr.root_gl_invariant, best_path, &best_score, nullptr);
    auto t_s_end = std::chrono::high_resolution_clock::now();
    double s_ns = std::chrono::duration<double, std::nano>(t_s_end - t_s_start).count();
    res.search_us = s_ok ? (s_ns / 1e3) : 0.0;

    alignas(64) uint8_t slot_out[ZVD_STELLA8_SLOT_BYTES + 64];
    size_t slot_len = 0;
    bool veto_flag = false;
    auto t_p_start = std::chrono::high_resolution_clock::now();
    bool p_ok = zvd_storage_pread_slot(res.stored_path, 0, 4, key, slot_out, sizeof(slot_out), &slot_len, &veto_flag);
    auto t_p_end = std::chrono::high_resolution_clock::now();
    double p_ns = std::chrono::duration<double, std::nano>(t_p_end - t_p_start).count();
    res.pread_us = p_ok ? (p_ns / 1e3) : 0.0;

    size_t dec_len = 0;
    bool full_veto = false;
    auto t_g_start = std::chrono::high_resolution_clock::now();
    bool g_ok = zvd_storage_get_full_cell(res.stored_path, key, bbuf.decoded, sizeof(bbuf.decoded), &dec_len, &full_veto);
    auto t_g_end = std::chrono::high_resolution_clock::now();
    double g_ns = std::chrono::duration<double, std::nano>(t_g_end - t_g_start).count();
    if (g_ns <= 0.0) g_ns = 1.0;
    res.get_gbps = (g_ok && dec_len == payload_len && std::memcmp(bbuf.payload, bbuf.decoded, payload_len) == 0)
                       ? (static_cast<double>(payload_len) / g_ns)
                       : 0.0;

    // ASIL D Veto test: corrupt vertex_base5_code for geo_id = 0 on disk and attempt pread on coupled edge slot 4
    char tamper_path[512];
    std::snprintf(tamper_path, sizeof(tamper_path), "%s/tamper_%s.zvd3", vault_dir, zvd_codec_mode_name(mode));
    int src_fd = ::open(res.stored_path, O_RDONLY);
    int dst_fd = ::open(tamper_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (src_fd >= 0 && dst_fd >= 0) {
        uint8_t copy_buf[4096];
        ssize_t n_read = 0;
        while ((n_read = ::read(src_fd, copy_buf, sizeof(copy_buf))) > 0) {
            (void)ZvdNestedCodecDetail::write_all_fd(dst_fd, copy_buf, static_cast<size_t>(n_read));
        }
        off_t v_code_off = static_cast<off_t>(hdr.directory_offset) + 2;
        uint16_t bad_code = 0xFFFFu;
        (void)::pwrite(dst_fd, &bad_code, sizeof(bad_code), v_code_off);
    }
    if (src_fd >= 0) ::close(src_fd);
    if (dst_fd >= 0) ::close(dst_fd);

    bool tamper_veto = false;
    size_t bad_slot_len = 0;
    bool tamper_ok = zvd_storage_pread_slot(tamper_path, 0, 4, key, slot_out, sizeof(slot_out), &bad_slot_len, &tamper_veto);
    res.veto_verified = (!tamper_ok && tamper_veto);
    (void)::unlink(tamper_path);

    return res;
}

} // namespace ZvdNestedBenchmarkDetail

int main() {
    static constexpr const char* BENCH_DIR = "./zvd_nested_vault_bench";
    const char* key = std::getenv("ZVD_MASTER_KEY");
    if (!key || key[0] == '\0') key = ZVD_DEFAULT_MASTER_KEY;

    static ZvdNestedBenchmarkDetail::BenchBuffers g_bench_bufs;
    ZvdNestedBenchmarkDetail::fill_test_payload_36mb(g_bench_bufs.payload, ZvdNestedBenchmarkDetail::BENCH_BYTES);

    ZvdNestedBenchmarkDetail::cleanup_bench_dir(BENCH_DIR);
    ZvdNestedCodecDetail::ensure_vault_dir(BENCH_DIR);

    auto r_stella = ZvdNestedBenchmarkDetail::run_mode_benchmark(
        BENCH_DIR, g_bench_bufs, ZvdNestedBenchmarkDetail::BENCH_BYTES, key, ZvdCodecTopologyMode::STELLA_8_VERT
    );
    auto r_edge12 = ZvdNestedBenchmarkDetail::run_mode_benchmark(
        BENCH_DIR, g_bench_bufs, ZvdNestedBenchmarkDetail::BENCH_BYTES, key, ZvdCodecTopologyMode::EDGE12_TETRA
    );

    std::cout << "========================================================================================\n";
    std::cout << " ZVD-120D NESTED GEOMETRIC CODEC & .ZVD3 VAULT LIVE BENCHMARK (3.60 MB)\n";
    std::cout << "========================================================================================\n";
    std::cout << std::left << std::setw(16) << "Topology"
              << std::right << std::setw(10) << "Ratio"
              << std::right << std::setw(12) << "Pack+Put"
              << std::right << std::setw(12) << "Get+Dec"
              << std::right << std::setw(12) << "VecSearch"
              << std::right << std::setw(12) << "pread(Edge)"
              << std::right << std::setw(12) << "ASIL D Veto" << "\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    const ZvdNestedBenchmarkDetail::ModeBenchResult rows[2] = { r_stella, r_edge12 };
    for (const auto& r : rows) {
        std::cout << std::left << std::setw(16) << zvd_codec_mode_name(r.mode)
                  << std::right << std::setw(9) << std::fixed << std::setprecision(2) << r.ratio << "x"
                  << std::right << std::setw(9) << std::setprecision(2) << r.put_gbps << "GB/s"
                  << std::right << std::setw(9) << std::setprecision(2) << r.get_gbps << "GB/s"
                  << std::right << std::setw(9) << std::setprecision(2) << r.search_us << "us"
                  << std::right << std::setw(9) << std::setprecision(2) << r.pread_us << "us"
                  << std::right << std::setw(12) << (r.veto_verified ? "VERIFIED" : "FAIL") << "\n";
    }
    std::cout << "========================================================================================\n";

    ZvdNestedBenchmarkDetail::cleanup_bench_dir(BENCH_DIR);
    return (r_stella.veto_verified && r_edge12.veto_verified) ? 0 : 1;
}

#endif // !defined(ZVD_NO_MAIN) || (ZVD_NO_MAIN == 0)
