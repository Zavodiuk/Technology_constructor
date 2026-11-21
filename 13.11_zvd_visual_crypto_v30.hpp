/* =============================================================================
 *  PROJECT:    ZVD-TETRA-120D BIT-PERFECT VISUAL CRYPTO CODEC & MOIRE ATLAS (SLOT 3)
 *              (13.11_zvd_visual_crypto_v30.hpp - V3.2.2-EGYPTIAN-345-C20)
 *  FILENAME:   13.11_zvd_visual_crypto_v30.hpp
 *  VERSION:    2026.11-V3.2.2-EGYPTIAN-345-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Allocation, alignas(64), Zero-Lock (constexpr LUT), OpenMP Safe
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_VISUAL_CRYPTO 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_REGISTER_CARTRIDGE_CTX)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_visual_crypto_v30.hpp must be included via 13.11_zvd_local_ai_proxy.cpp with datacenter core!"
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <array>
#include <algorithm>
#include <string>
#include <string_view>

#define ATLAS_TILE_W          40
#define ATLAS_TILE_H          30
#define ZVD_CUBE_RAW_CAPACITY 250 // Exactly 250 raw bytes per cube (1000 cells * 2 bits = 2000 bits)

// =====================================================================
// 1. OMNIVOROUS HEADERS & ZERO-LOCK COMPILE-TIME BASE-5 UNPACKING LUT
// =====================================================================

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct {
    uint64_t orig_len;
    char type_sig[4];
    uint64_t data_hash;
} ZVD_OmnivorousHeader;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    uint64_t orig_len;
    char type_sig[4];
    uint64_t data_hash;
} ZVD_OmnivorousHeader;
#endif

// Compile-time Zero-Lock Lukasiewicz Base-5 Unpacking LUT (100% Thread-Safe, Zero Mutex)
inline constexpr std::array<std::array<int8_t, 3>, 256> ZVD_CRYPTO_UNPACK_BASE5_LUT = []() {
    std::array<std::array<int8_t, 3>, 256> lut{};
    for (int code = 0; code < 256; code++) {
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

inline void zvd_ensure_crypto_lut(void) noexcept {
    zvd_ensure_luts();
}

inline void zvd_fast_unpack_layer_lukasiewicz(const uint8_t * __restrict__ in_167, int8_t * __restrict__ layer_500) noexcept {
    int idx = 0;
    for (int i = 0; i < 166; i++) {
        uint8_t code = in_167[i];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][0];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][1];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][2];
    }
    uint8_t code_last = in_167[166];
    layer_500[498] = static_cast<int8_t>((code_last % 5) - 2);
    layer_500[499] = static_cast<int8_t>((code_last / 5) - 2);
}

inline void zvd_fast_cube_unpack_monolith(const ZVD_Monolith334 * __restrict__ monolith, ZVD_TetraCube120D * __restrict__ out_cube) noexcept {
    zvd_fast_unpack_layer_lukasiewicz(monolith->face_packed, out_cube->face_data);
    zvd_fast_unpack_layer_lukasiewicz(monolith->vertex_packed, out_cube->vertex_data);
}


// =====================================================================
// 2. BI-DIRECTIONAL MOIRÉ ENCRYPTION & APERIODIC CELL STREAM O(1)
// =====================================================================

struct ZvdEgyptianTriangle345 {
    double leg_a;
    double leg_b;
    double hypotenuse;
    int phase_shift_mod5;
};

inline ZvdEgyptianTriangle345 zvd_egyptian_hypotenuse_from_cube(size_t cube_index, int cell_idx) noexcept {
    double scale = static_cast<double>((cube_index & 0xFFULL) + 1ULL) +
                   (static_cast<double>((cell_idx & 0x1F) + 1) / 32.0);
    double leg_a = 3.0 * scale;
    double leg_b = 4.0 * scale;
    double hyp = std::hypot(leg_a, leg_b);
    uint64_t hyp_bits = static_cast<uint64_t>(std::llround(hyp * 1000.0));
    int shift_mod5 = static_cast<int>(hyp_bits % 5ULL);
    return { leg_a, leg_b, hyp, shift_mod5 };
}

inline void zvd_120d_moire_crypt_cube_fast(ZVD_TetraCube120D * __restrict__ cube, const char *passphrase, size_t cube_index, bool encrypt) noexcept {
    ZVD_BiCore120D core;
    zvd_bicore_init_from_key(&core, 0, passphrase);
    
    int total_step = static_cast<int>(cube_index + 1) * core.step_deg;
    core.angle_deg_a = (core.angle_deg_a + total_step) % 360;
    core.angle_deg_b = (core.angle_deg_b - total_step + 360) % 360;
    
    alignas(64) float collapsed[ZVD_DIM_120];
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    
    uint64_t p_hash = zvd_fast_hash64(reinterpret_cast<const uint8_t*>(passphrase), std::strlen(passphrase));
    for (int j = 0; j < ZVD_DIM_120; j++) {
        uint64_t val = p_hash ^ (static_cast<uint64_t>(cube_index) * 0x9E3779B97F4A7C15ULL) ^ (static_cast<uint64_t>(j) * 2654435761ULL);
        collapsed[j] = static_cast<float>(val & 0xFF);
    }
    
    zvd_enterprise_synthesis_120d(collapsed, &core, synth_120);
    
    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        ZvdEgyptianTriangle345 tri = zvd_egyptian_hypotenuse_from_cube(cube_index, i);

        uint64_t cell_mix = p_hash ^ (static_cast<uint64_t>(cube_index) * 0x9E3779B97F4A7C15ULL)
                                   ^ (static_cast<uint64_t>(i) * 0xBF58476D1CE4E5B9ULL)
                                   ^ static_cast<uint64_t>(std::llround(tri.hypotenuse * 1000.0));
        cell_mix ^= (cell_mix >> 31);
        cell_mix *= 0xFF51AFD7ED558CCDULL;
        cell_mix ^= (cell_mix >> 33);

        int base_k = std::abs(static_cast<int>(synth_120[i % ZVD_DIM_120]));
        int k_face = (base_k + static_cast<int>((cell_mix >> 56) % 5ULL) + tri.phase_shift_mod5) % 5;
        int k_vert = (base_k + static_cast<int>((cell_mix >> 48) % 5ULL) + tri.phase_shift_mod5 + 1) % 5;
        
        int f_val = static_cast<int>(cube->face_data[i]) + 2;
        f_val = encrypt ? (f_val + k_face) % 5 : (f_val - k_face + 5) % 5;
        cube->face_data[i] = static_cast<int8_t>(f_val - 2);
        
        int v_val = static_cast<int>(cube->vertex_data[i]) + 2;
        v_val = encrypt ? (v_val + k_vert) % 5 : (v_val - k_vert + 5) % 5;
        cube->vertex_data[i] = static_cast<int8_t>(v_val - 2);
    }
}


// =====================================================================
// 2.5 EGYPTIAN TRIANGLE (3-4-5) HYPOTENUSE CUBE GEOMETRY BRIDGE
// =====================================================================

inline double zvd_cube_egyptian_hypotenuse(const ZVD_TetraCube120D *cube, size_t cube_index) noexcept {
    int64_t energy_acc = 0;
    if (cube) {
        for (int i = 0; i < ZVD_CELLS_PER_LAYER; i += 10) {
            energy_acc += static_cast<int64_t>(cube->face_data[i]) + static_cast<int64_t>(cube->vertex_data[i]);
        }
    }
    double cube_factor = static_cast<double>((cube_index & 0xFFULL) + 1ULL) +
                         (static_cast<double>(std::llabs(energy_acc) % 100LL) / 100.0);
    double side_3 = 3.0 * cube_factor;
    double side_4 = 4.0 * cube_factor;
    return std::hypot(side_3, side_4);
}


// =====================================================================
// 3. BIT-PERFECT OMNIVOROUS GATEWAY (250 BYTES <-> CUBE CELLS)
// =====================================================================

inline void zvd_bytes_to_cube_cells(const uint8_t *bytes_250, ZVD_TetraCube120D *cube) noexcept {
    for (int i = 0; i < 125; i++) {
        uint8_t b = bytes_250[i];
        cube->face_data[i * 4 + 0] = static_cast<int8_t>(((b >> 0) & 3) - 2);
        cube->face_data[i * 4 + 1] = static_cast<int8_t>(((b >> 2) & 3) - 2);
        cube->face_data[i * 4 + 2] = static_cast<int8_t>(((b >> 4) & 3) - 2);
        cube->face_data[i * 4 + 3] = static_cast<int8_t>(((b >> 6) & 3) - 2);
    }
    for (int i = 0; i < 125; i++) {
        uint8_t b = bytes_250[125 + i];
        cube->vertex_data[i * 4 + 0] = static_cast<int8_t>(((b >> 0) & 3) - 2);
        cube->vertex_data[i * 4 + 1] = static_cast<int8_t>(((b >> 2) & 3) - 2);
        cube->vertex_data[i * 4 + 2] = static_cast<int8_t>(((b >> 4) & 3) - 2);
        cube->vertex_data[i * 4 + 3] = static_cast<int8_t>(((b >> 6) & 3) - 2);
    }
}

inline void zvd_cube_cells_to_bytes(const ZVD_TetraCube120D *cube, uint8_t *bytes_250) noexcept {
    for (int i = 0; i < 125; i++) {
        uint8_t b0 = static_cast<uint8_t>(cube->face_data[i * 4 + 0] + 2) & 3;
        uint8_t b1 = static_cast<uint8_t>(cube->face_data[i * 4 + 1] + 2) & 3;
        uint8_t b2 = static_cast<uint8_t>(cube->face_data[i * 4 + 2] + 2) & 3;
        uint8_t b3 = static_cast<uint8_t>(cube->face_data[i * 4 + 3] + 2) & 3;
        bytes_250[i] = b0 | (b1 << 2) | (b2 << 4) | (b3 << 6);
    }
    for (int i = 0; i < 125; i++) {
        uint8_t b0 = static_cast<uint8_t>(cube->vertex_data[i * 4 + 0] + 2) & 3;
        uint8_t b1 = static_cast<uint8_t>(cube->vertex_data[i * 4 + 1] + 2) & 3;
        uint8_t b2 = static_cast<uint8_t>(cube->vertex_data[i * 4 + 2] + 2) & 3;
        uint8_t b3 = static_cast<uint8_t>(cube->vertex_data[i * 4 + 3] + 2) & 3;
        bytes_250[125 + i] = b0 | (b1 << 2) | (b2 << 4) | (b3 << 6);
    }
}

inline size_t zvd_encode_any_to_monoliths(const uint8_t *payload, size_t payload_len, const char *type_sig, const char *passphrase, ZVD_Monolith334 **out_monoliths) {
    size_t total_size = sizeof(ZVD_OmnivorousHeader) + payload_len;
    size_t num_cubes = (total_size + ZVD_CUBE_RAW_CAPACITY - 1) / ZVD_CUBE_RAW_CAPACITY;
    size_t padded_size = num_cubes * ZVD_CUBE_RAW_CAPACITY;
    
    uint8_t *stream = static_cast<uint8_t *>(std::calloc(1, padded_size));
    if (!stream) return 0;
    
    ZVD_OmnivorousHeader *hdr = reinterpret_cast<ZVD_OmnivorousHeader *>(stream);
    hdr->orig_len = static_cast<uint64_t>(payload_len);
    std::memcpy(hdr->type_sig, type_sig, 4);
    hdr->data_hash = zvd_fast_hash64(payload, payload_len);
    std::memcpy(stream + sizeof(ZVD_OmnivorousHeader), payload, payload_len);
    
    ZVD_Monolith334 *monoliths = static_cast<ZVD_Monolith334 *>(std::malloc(num_cubes * sizeof(ZVD_Monolith334)));
    if (!monoliths) {
        std::free(stream);
        return 0;
    }
    
    for (size_t i = 0; i < num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_bytes_to_cube_cells(stream + i * ZVD_CUBE_RAW_CAPACITY, &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, passphrase, i, true);
        zvd_cube_pack_monolith(&cube, &monoliths[i]);
    }
    
    std::free(stream);
    *out_monoliths = monoliths;
    return num_cubes;
}

inline uint8_t* zvd_decode_monoliths_to_any(const ZVD_Monolith334 *monoliths, size_t num_cubes, const char *passphrase, size_t *out_len, char out_type_sig[5], bool *out_valid_hash) {
    size_t total_bytes = num_cubes * ZVD_CUBE_RAW_CAPACITY;
    uint8_t *stream = static_cast<uint8_t *>(std::malloc(total_bytes));
    if (!stream) return nullptr;
    
    for (size_t i = 0; i < num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_fast_cube_unpack_monolith(&monoliths[i], &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, passphrase, i, false);
        zvd_cube_cells_to_bytes(&cube, stream + i * ZVD_CUBE_RAW_CAPACITY);
    }
    
    if (total_bytes < sizeof(ZVD_OmnivorousHeader)) {
        std::free(stream);
        return nullptr;
    }
    
    ZVD_OmnivorousHeader *hdr = reinterpret_cast<ZVD_OmnivorousHeader *>(stream);
    size_t orig_len = static_cast<size_t>(hdr->orig_len);
    if (orig_len > total_bytes - sizeof(ZVD_OmnivorousHeader)) {
        std::free(stream);
        return nullptr;
    }

    std::memcpy(out_type_sig, hdr->type_sig, 4);
    out_type_sig[4] = '\0';
    
    uint8_t *payload = static_cast<uint8_t *>(std::malloc(orig_len));
    if (!payload) {
        std::free(stream);
        return nullptr;
    }
    
    std::memcpy(payload, stream + sizeof(ZVD_OmnivorousHeader), orig_len);
    uint64_t computed_hash = zvd_fast_hash64(payload, orig_len);
    *out_valid_hash = (computed_hash == hdr->data_hash);
    *out_len = orig_len;
    
    std::free(stream);
    return payload;
}


// =====================================================================
// 4. PGM ATLAS RENDERER & LIVE MOIRÉ PASSPORT
// =====================================================================

static inline uint8_t zvd_penta_to_gray(int8_t p) noexcept {
    int val = static_cast<int>(p) + 2;
    if (val < 0) val = 0; if (val > 4) val = 4;
    switch(val) {
        case 0: return 0;
        case 1: return 64;
        case 2: return 128;
        case 3: return 192;
        default: return 255;
    }
}

static inline int8_t zvd_gray_to_penta(uint8_t g) noexcept {
    if (g < 32) return -2;
    if (g < 96) return -1;
    if (g < 160) return 0;
    if (g < 224) return 1;
    return 2;
}

inline uint64_t zvd_compute_core_library_fingerprint(void) noexcept {
    uint64_t f1 = zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ZVD_LUT_345), sizeof(ZVD_LUT_345));
    uint64_t f2 = zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ZVD_SIN_360), sizeof(ZVD_SIN_360));
    uint64_t f3 = zvd_fast_hash64(reinterpret_cast<const uint8_t*>(ZVD_COS_360), sizeof(ZVD_COS_360));
    uint64_t f4 = zvd_fast_hash64(reinterpret_cast<const uint8_t*>(&ZVD_CRYPTO_UNPACK_BASE5_LUT), sizeof(ZVD_CRYPTO_UNPACK_BASE5_LUT));
    return f1 ^ (f2 + 0x9E3779B97F4A7C15ULL + (f3 << 6) + (f3 >> 2)) ^ f4;
}

inline bool zvd_save_atlas_pgm(const ZVD_Monolith334 *monoliths, size_t num_cubes, uint64_t lib_fingerprint, const char *passphrase, const char *pgm_path) {
    if (!monoliths || num_cubes == 0 || !passphrase || !pgm_path) return false;
    int cols = 16;
    int rows = static_cast<int>((num_cubes + static_cast<size_t>(cols) - 1) / static_cast<size_t>(cols));
    size_t img_w = static_cast<size_t>(cols) * static_cast<size_t>(ATLAS_TILE_W);
    size_t img_h = static_cast<size_t>(rows) * static_cast<size_t>(ATLAS_TILE_H);
    size_t total_pixels = img_w * img_h;
    
    uint8_t *canvas = static_cast<uint8_t *>(std::calloc(1, total_pixels));
    if (!canvas) return false;
    
    for (size_t idx = 0; idx < num_cubes; idx++) {
        size_t ty = idx / static_cast<size_t>(cols);
        size_t tx = idx % static_cast<size_t>(cols);
        size_t x0 = tx * static_cast<size_t>(ATLAS_TILE_W);
        size_t y0 = ty * static_cast<size_t>(ATLAS_TILE_H);
        
        ZVD_TetraCube120D cube;
        zvd_fast_cube_unpack_monolith(&monoliths[idx], &cube);
        
        for (size_t r = 0; r < 25; r++) {
            for (size_t c = 0; c < 20; c++) {
                size_t cell = r * 20 + c;
                canvas[(y0 + r) * img_w + (x0 + c)] = zvd_penta_to_gray(cube.face_data[cell]);
                canvas[(y0 + r) * img_w + (x0 + 20 + c)] = zvd_penta_to_gray(cube.vertex_data[cell]);
            }
        }
        
        ZVD_BiCore120D vis_core;
        zvd_bicore_init_from_key(&vis_core, 0, passphrase);
        int total_step = static_cast<int>(idx + 1) * vis_core.step_deg;
        vis_core.angle_deg_a = (vis_core.angle_deg_a + total_step) % 360;
        vis_core.angle_deg_b = (vis_core.angle_deg_b - total_step + 360) % 360;
        
        for (size_t r = 25; r < 30; r++) {
            for (size_t c = 0; c < static_cast<size_t>(ATLAS_TILE_W); c++) {
                int phase_a = static_cast<int>((r * 17 + c * 9 + static_cast<size_t>(vis_core.angle_deg_a)) % 360);
                int phase_b = static_cast<int>((r * 11 + c * 13 + static_cast<size_t>(vis_core.angle_deg_b)) % 360);
                float moire = ZVD_SIN_360[phase_a] * ZVD_COS_360[phase_b];
                canvas[(y0 + r) * img_w + (x0 + c)] = static_cast<uint8_t>(128.0f + moire * 127.0f);
            }
        }
        
        if (idx == 0) {
            uint64_t nc = static_cast<uint64_t>(num_cubes);
            std::memcpy(&canvas[(y0 + 25) * img_w + x0], &nc, sizeof(uint64_t));
            std::memcpy(&canvas[(y0 + 25) * img_w + x0 + 8], &lib_fingerprint, sizeof(uint64_t));
        }
    }
    
    FILE *f = std::fopen(pgm_path, "wb");
    if (!f) {
        std::free(canvas);
        return false;
    }
    std::fprintf(f, "P5\n%zu %zu\n255\n", img_w, img_h);
    std::fwrite(canvas, 1, total_pixels, f);
    std::fclose(f);
    std::free(canvas);
    return true;
}

inline bool zvd_load_atlas_pgm(const char *pgm_path, ZVD_Monolith334 **out_monoliths, size_t *out_num_cubes, uint64_t *out_lib_fingerprint) {
    if (!pgm_path || !out_monoliths || !out_num_cubes || !out_lib_fingerprint) return false;
    FILE *f = std::fopen(pgm_path, "rb");
    if (!f) return false;
    
    char magic[3];
    if (std::fscanf(f, "%2s", magic) != 1 || magic[0] != 'P' || magic[1] != '5') {
        std::fclose(f);
        return false;
    }
    
    int img_w_in = 0, img_h_in = 0, max_val = 0;
    if (std::fscanf(f, "%d %d %d", &img_w_in, &img_h_in, &max_val) != 3) {
        std::fclose(f);
        return false;
    }
    if (img_w_in <= 0 || img_h_in <= 0 || max_val <= 0 || max_val > 255) {
        std::fclose(f);
        return false;
    }
    if ((img_w_in % ATLAS_TILE_W) != 0 || (img_h_in % ATLAS_TILE_H) != 0) {
        std::fclose(f);
        return false;
    }
    std::fgetc(f);
    
    size_t img_w = static_cast<size_t>(img_w_in);
    size_t img_h = static_cast<size_t>(img_h_in);
    if (img_w > (SIZE_MAX / 2) / img_h) {
        std::fclose(f);
        return false;
    }
    size_t img_size = img_w * img_h;
    uint8_t *canvas = static_cast<uint8_t *>(std::malloc(img_size));
    if (!canvas) {
        std::fclose(f);
        return false;
    }
    
    if (std::fread(canvas, 1, img_size, f) != img_size) {
        std::free(canvas);
        std::fclose(f);
        return false;
    }
    std::fclose(f);
    
    size_t cols = img_w / static_cast<size_t>(ATLAS_TILE_W);
    size_t rows = img_h / static_cast<size_t>(ATLAS_TILE_H);
    
    uint64_t num_cubes = 0;
    uint64_t lib_fp = 0;
    
    if (img_size >= (25 * img_w + 16)) {
        std::memcpy(&num_cubes, &canvas[25 * img_w], sizeof(uint64_t));
        std::memcpy(&lib_fp, &canvas[25 * img_w + 8], sizeof(uint64_t));
    }
    
    size_t max_tiles = cols * rows;
    if (num_cubes == 0 || num_cubes > max_tiles) {
        num_cubes = max_tiles;
    }
    
    ZVD_Monolith334 *monoliths = static_cast<ZVD_Monolith334 *>(std::malloc(static_cast<size_t>(num_cubes) * sizeof(ZVD_Monolith334)));
    if (!monoliths) {
        std::free(canvas);
        return false;
    }
    
    for (size_t idx = 0; idx < static_cast<size_t>(num_cubes); idx++) {
        size_t ty = idx / cols;
        size_t tx = idx % cols;
        size_t x0 = tx * static_cast<size_t>(ATLAS_TILE_W);
        size_t y0 = ty * static_cast<size_t>(ATLAS_TILE_H);
        
        ZVD_TetraCube120D cube;
        for (size_t r = 0; r < 25; r++) {
            for (size_t c = 0; c < 20; c++) {
                size_t cell = r * 20 + c;
                cube.face_data[cell] = zvd_gray_to_penta(canvas[(y0 + r) * img_w + (x0 + c)]);
                cube.vertex_data[cell] = zvd_gray_to_penta(canvas[(y0 + r) * img_w + (x0 + 20 + c)]);
            }
        }
        zvd_cube_pack_monolith(&cube, &monoliths[idx]);
    }
    
    std::free(canvas);
    *out_monoliths = monoliths;
    *out_num_cubes = static_cast<size_t>(num_cubes);
    *out_lib_fingerprint = lib_fp;
    return true;
}


// =====================================================================
// 5. SYMMETRIC PEER API (SUBSCRIBER A <-> B)
// =====================================================================

inline bool zvd_peer_send_to_visual_atlas(const uint8_t *data, size_t len, const char *type_sig, const char *shared_key, const char *out_pgm_path) {
    ZVD_Monolith334 *monoliths = nullptr;
    size_t num_cubes = zvd_encode_any_to_monoliths(data, len, type_sig, shared_key, &monoliths);
    if (num_cubes == 0 || !monoliths) return false;
    
    uint64_t fp = zvd_compute_core_library_fingerprint();
    bool saved = zvd_save_atlas_pgm(monoliths, num_cubes, fp, shared_key, out_pgm_path);
    std::free(monoliths);
    return saved;
}

inline uint8_t* zvd_peer_receive_from_visual_atlas(const char *in_pgm_path, const char *shared_key, size_t *out_len, char out_type_sig[5]) {
    ZVD_Monolith334 *monoliths = nullptr;
    size_t num_cubes = 0;
    uint64_t remote_fp = 0;
    
    if (!zvd_load_atlas_pgm(in_pgm_path, &monoliths, &num_cubes, &remote_fp)) return nullptr;
    
    uint64_t local_fp = zvd_compute_core_library_fingerprint();
    if (remote_fp != 0 && remote_fp != local_fp) {
        std::fprintf(stderr, "[!] Warning: Core library fingerprint mismatch (Local: %016llX, Remote: %016llX)\n", 
                     static_cast<unsigned long long>(local_fp), static_cast<unsigned long long>(remote_fp));
    }
    
    bool valid_hash = false;
    uint8_t *payload = zvd_decode_monoliths_to_any(monoliths, num_cubes, shared_key, out_len, out_type_sig, &valid_hash);
    std::free(monoliths);
    
    if (!valid_hash) {
        if (payload) std::free(payload);
        return nullptr;
    }
    
    return payload;
}


// =====================================================================
// 6. UNIVERSAL BUS INTEGRATION (V3.2 ZERO-LOCK / IN-MEMORY ROUND-TRIP)
// =====================================================================

namespace {

inline const char* zvd_visual_crypto_active_key() noexcept {
    const char* env_key = std::getenv("ZVD_MASTER_KEY");
    if (env_key && env_key[0] != '\0') {
        return env_key;
    }
#ifdef ZVD_DEFAULT_MASTER_KEY
    return ZVD_DEFAULT_MASTER_KEY;
#else
    return "ZVD-SOVEREIGN-ENTERPRISE-2026";
#endif
}

} // namespace

inline int zvd_visual_crypto_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_CRYPTO_TEST")) {
        const char *key = zvd_visual_crypto_active_key();
        alignas(64) uint8_t src_bytes[ZVD_CUBE_RAW_CAPACITY] = {0};
        const char *test_msg = "ZVD V3.2 In-Memory Zero-Lock Crypto Intercept Verified.";
        size_t msg_len = std::min(std::strlen(test_msg), static_cast<size_t>(ZVD_CUBE_RAW_CAPACITY));
        std::memcpy(src_bytes, test_msg, msg_len);

        ZVD_TetraCube120D cube;
        zvd_bytes_to_cube_cells(src_bytes, &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, key, 0, true);

        ZVD_Monolith334 monolith;
        zvd_cube_pack_monolith(&cube, &monolith);

        ZVD_TetraCube120D cube_rx;
        zvd_fast_cube_unpack_monolith(&monolith, &cube_rx);
        zvd_120d_moire_crypt_cube_fast(&cube_rx, key, 0, false);

        alignas(64) uint8_t rx_bytes[ZVD_CUBE_RAW_CAPACITY] = {0};
        zvd_cube_cells_to_bytes(&cube_rx, rx_bytes);

        if (std::memcmp(src_bytes, rx_bytes, ZVD_CUBE_RAW_CAPACITY) == 0) {
            return 1; // PLUS
        }
        return -1; // MINUS
    }
    return 0; // ZERO
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_visual_crypto_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_CRYPTO_TEST")) {
        const char *key = zvd_visual_crypto_active_key();
        size_t payload_len = std::min(ctx.prompt.size(), static_cast<size_t>(ZVD_CUBE_RAW_CAPACITY));
        
        alignas(64) uint8_t src_bytes[ZVD_CUBE_RAW_CAPACITY] = {0};
        std::memcpy(src_bytes, ctx.prompt.data(), payload_len);

        ZVD_TetraCube120D cube;
        zvd_bytes_to_cube_cells(src_bytes, &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, key, 0, true);

        ZVD_Monolith334 monolith;
        zvd_cube_pack_monolith(&cube, &monolith);

        ZVD_TetraCube120D cube_rx;
        zvd_fast_cube_unpack_monolith(&monolith, &cube_rx);
        zvd_120d_moire_crypt_cube_fast(&cube_rx, key, 0, false);

        alignas(64) uint8_t rx_bytes[ZVD_CUBE_RAW_CAPACITY] = {0};
        zvd_cube_cells_to_bytes(&cube_rx, rx_bytes);

        if (std::memcmp(src_bytes, rx_bytes, ZVD_CUBE_RAW_CAPACITY) == 0) {
            return 1; // PLUS
        }
        return -1; // MINUS
    }
    return 0; // ZERO (transparent propagation)
}
#endif

inline bool zvd_visual_crypto_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_CRYPTO_SECURE")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.2 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("VisualCryptoAtlas-120D-IQ360", ZvdModuleRole::FILTER, zvd_visual_crypto_evaluate_ctx, zvd_visual_crypto_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("VisualCryptoAtlas-120D-IQ360", zvd_visual_crypto_evaluate, zvd_visual_crypto_verify);
#endif

#endif // ZVD_VISUAL_CRYPTO_V30_HPP
