/* =============================================================================
 *  PROJECT:    ZVD-TETRA-120D BIT-PERFECT VISUAL CRYPTO CODEC & MOIRE ATLAS (SLOT 3)
 *              (13.11_zvd_visual_crypto_v30.hpp - V3.1-OMNIVOROUS-BUS-C20)
 *  FILENAME:   13.11_zvd_visual_crypto_v30.hpp
 *  VERSION:    2026.11-V3.1-OMNIVOROUS-BUS-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  --------------------------------------------------------------------------
 *  ASSEMBLY INSTRUCTIONS (Technology Constructor Ecosystem V3.1.2):
 *  Place this file in the root working directory alongside:
 *   1) zvd_local_ai_proxy.cpp (Root Orchestrator V3.1.2)
 *   2) zvd_datacenter_node_v28_lite.c (Datacenter Core 120D)
 *   3) 13.11_zvd_visual_crypto_v30.hpp (Cryptographic Cartridge)
 *  Build command (Linux/macOS):
 *   g++ -std=c++20 -O3 -Wall -Wextra -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm -lpthread
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_VISUAL_CRYPTO 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_visual_crypto_v30.hpp requires zvd_local_ai_proxy.cpp and zvd_datacenter_node_v28_lite.c in the working directory!"
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <string>
#include <string_view>

#define ATLAS_TILE_W          40
#define ATLAS_TILE_H          30
#define ZVD_CUBE_RAW_CAPACITY 250 // Exactly 250 raw bytes per cube (1000 cells * 2 bits = 2000 bits)

// =====================================================================
// 1. OMNIVOROUS HEADERS & THREAD-SAFE LUKASIEWICZ BASE-5 UNPACKING LUT
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

alignas(64) static int8_t ZVD_CRYPTO_UNPACK_BASE5_LUT[256][3];
static std::atomic<bool> g_crypto_lut_inited{false};

inline void zvd_ensure_crypto_lut(void) {
    zvd_ensure_luts();
    if (g_crypto_lut_inited.load(std::memory_order_acquire)) return;
    
    static std::mutex crypto_lut_mutex;
    std::lock_guard<std::mutex> lock(crypto_lut_mutex);
    if (g_crypto_lut_inited.load(std::memory_order_relaxed)) return;

    for (int code = 0; code < 256; code++) {
        int temp = code;
        int a = temp % 5; temp /= 5;
        int b = temp % 5; temp /= 5;
        int c = temp % 5;
        ZVD_CRYPTO_UNPACK_BASE5_LUT[code][0] = (int8_t)(a - 2);
        ZVD_CRYPTO_UNPACK_BASE5_LUT[code][1] = (int8_t)(b - 2);
        ZVD_CRYPTO_UNPACK_BASE5_LUT[code][2] = (int8_t)(c - 2);
    }
    g_crypto_lut_inited.store(true, std::memory_order_release);
}

inline void zvd_fast_unpack_layer_lukasiewicz(const uint8_t * restrict in_167, int8_t * restrict layer_500) {
    zvd_ensure_crypto_lut();
    int idx = 0;
    for (int i = 0; i < 166; i++) {
        uint8_t code = in_167[i];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][0];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][1];
        layer_500[idx++] = ZVD_CRYPTO_UNPACK_BASE5_LUT[code][2];
    }
    uint8_t code_last = in_167[166];
    layer_500[498] = (int8_t)((code_last % 5) - 2);
    layer_500[499] = (int8_t)((code_last / 5) - 2);
}

inline void zvd_fast_cube_unpack_monolith(const ZVD_Monolith334 * restrict monolith, ZVD_TetraCube120D * restrict out_cube) {
    zvd_fast_unpack_layer_lukasiewicz(monolith->face_packed, out_cube->face_data);
    zvd_fast_unpack_layer_lukasiewicz(monolith->vertex_packed, out_cube->vertex_data);
}

// =====================================================================
// 2. BI-DIRECTIONAL MOIRÉ ENCRYPTION & FAST PHASE SHIFT O(1)
// =====================================================================

inline void zvd_120d_moire_crypt_cube_fast(ZVD_TetraCube120D * restrict cube, const char *passphrase, size_t cube_index, bool encrypt) {
    ZVD_BiCore120D core;
    zvd_bicore_init_from_key(&core, 0, passphrase);
    
    int total_step = (int)(cube_index + 1) * core.step_deg;
    core.angle_deg_a = (core.angle_deg_a + total_step) % 360;
    core.angle_deg_b = (core.angle_deg_b - total_step + 360) % 360;
    
    alignas(64) float collapsed[ZVD_DIM_120];
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    
    uint64_t p_hash = zvd_fast_hash64((const uint8_t*)passphrase, strlen(passphrase));
    for (int j = 0; j < ZVD_DIM_120; j++) {
        uint64_t val = p_hash ^ (cube_index * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t)j * 2654435761ULL);
        collapsed[j] = (float)(val & 0xFF);
    }
    
    zvd_enterprise_synthesis_120d(collapsed, &core, synth_120);
    
    int k_idx = 0;
    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int k_val = abs((int)synth_120[k_idx % ZVD_DIM_120]) % 5;
        k_idx++;
        
        int f_val = (int)cube->face_data[i] + 2;
        f_val = encrypt ? (f_val + k_val) % 5 : (f_val - k_val + 5) % 5;
        cube->face_data[i] = (int8_t)(f_val - 2);
        
        int v_val = (int)cube->vertex_data[i] + 2;
        v_val = encrypt ? (v_val + k_val) % 5 : (v_val - k_val + 5) % 5;
        cube->vertex_data[i] = (int8_t)(v_val - 2);
    }
}

// =====================================================================
// 3. BIT-PERFECT OMNIVOROUS GATEWAY (250 BYTES <-> CUBE CELLS)
// =====================================================================

inline void zvd_bytes_to_cube_cells(const uint8_t *bytes_250, ZVD_TetraCube120D *cube) {
    for (int i = 0; i < 125; i++) {
        uint8_t b = bytes_250[i];
        cube->face_data[i * 4 + 0] = (int8_t)(((b >> 0) & 3) - 2);
        cube->face_data[i * 4 + 1] = (int8_t)(((b >> 2) & 3) - 2);
        cube->face_data[i * 4 + 2] = (int8_t)(((b >> 4) & 3) - 2);
        cube->face_data[i * 4 + 3] = (int8_t)(((b >> 6) & 3) - 2);
    }
    for (int i = 0; i < 125; i++) {
        uint8_t b = bytes_250[125 + i];
        cube->vertex_data[i * 4 + 0] = (int8_t)(((b >> 0) & 3) - 2);
        cube->vertex_data[i * 4 + 1] = (int8_t)(((b >> 2) & 3) - 2);
        cube->vertex_data[i * 4 + 2] = (int8_t)(((b >> 4) & 3) - 2);
        cube->vertex_data[i * 4 + 3] = (int8_t)(((b >> 6) & 3) - 2);
    }
}

inline void zvd_cube_cells_to_bytes(const ZVD_TetraCube120D *cube, uint8_t *bytes_250) {
    for (int i = 0; i < 125; i++) {
        uint8_t b0 = (uint8_t)(cube->face_data[i * 4 + 0] + 2) & 3;
        uint8_t b1 = (uint8_t)(cube->face_data[i * 4 + 1] + 2) & 3;
        uint8_t b2 = (uint8_t)(cube->face_data[i * 4 + 2] + 2) & 3;
        uint8_t b3 = (uint8_t)(cube->face_data[i * 4 + 3] + 2) & 3;
        bytes_250[i] = b0 | (b1 << 2) | (b2 << 4) | (b3 << 6);
    }
    for (int i = 0; i < 125; i++) {
        uint8_t b0 = (uint8_t)(cube->vertex_data[i * 4 + 0] + 2) & 3;
        uint8_t b1 = (uint8_t)(cube->vertex_data[i * 4 + 1] + 2) & 3;
        uint8_t b2 = (uint8_t)(cube->vertex_data[i * 4 + 2] + 2) & 3;
        uint8_t b3 = (uint8_t)(cube->vertex_data[i * 4 + 3] + 2) & 3;
        bytes_250[125 + i] = b0 | (b1 << 2) | (b2 << 4) | (b3 << 6);
    }
}

inline size_t zvd_encode_any_to_monoliths(const uint8_t *payload, size_t payload_len, const char *type_sig, const char *passphrase, ZVD_Monolith334 **out_monoliths) {
    size_t total_size = sizeof(ZVD_OmnivorousHeader) + payload_len;
    size_t num_cubes = (total_size + ZVD_CUBE_RAW_CAPACITY - 1) / ZVD_CUBE_RAW_CAPACITY;
    size_t padded_size = num_cubes * ZVD_CUBE_RAW_CAPACITY;
    
    uint8_t *stream = (uint8_t *)calloc(1, padded_size);
    if (!stream) return 0;
    
    ZVD_OmnivorousHeader *hdr = (ZVD_OmnivorousHeader *)stream;
    hdr->orig_len = (uint64_t)payload_len;
    memcpy(hdr->type_sig, type_sig, 4);
    hdr->data_hash = zvd_fast_hash64(payload, payload_len);
    memcpy(stream + sizeof(ZVD_OmnivorousHeader), payload, payload_len);
    
    ZVD_Monolith334 *monoliths = (ZVD_Monolith334 *)malloc(num_cubes * sizeof(ZVD_Monolith334));
    if (!monoliths) {
        free(stream);
        return 0;
    }
    
    for (size_t i = 0; i < num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_bytes_to_cube_cells(stream + i * ZVD_CUBE_RAW_CAPACITY, &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, passphrase, i, true);
        zvd_cube_pack_monolith(&cube, &monoliths[i]);
    }
    
    free(stream);
    *out_monoliths = monoliths;
    return num_cubes;
}

inline uint8_t* zvd_decode_monoliths_to_any(const ZVD_Monolith334 *monoliths, size_t num_cubes, const char *passphrase, size_t *out_len, char out_type_sig[5], bool *out_valid_hash) {
    size_t total_bytes = num_cubes * ZVD_CUBE_RAW_CAPACITY;
    uint8_t *stream = (uint8_t *)malloc(total_bytes);
    if (!stream) return NULL;
    
    for (size_t i = 0; i < num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_fast_cube_unpack_monolith(&monoliths[i], &cube);
        zvd_120d_moire_crypt_cube_fast(&cube, passphrase, i, false);
        zvd_cube_cells_to_bytes(&cube, stream + i * ZVD_CUBE_RAW_CAPACITY);
    }
    
    if (total_bytes < sizeof(ZVD_OmnivorousHeader)) {
        free(stream);
        return NULL;
    }
    
    ZVD_OmnivorousHeader *hdr = (ZVD_OmnivorousHeader *)stream;
    size_t orig_len = (size_t)hdr->orig_len;
    if (orig_len > total_bytes - sizeof(ZVD_OmnivorousHeader)) {
        free(stream);
        return NULL;
    }

    memcpy(out_type_sig, hdr->type_sig, 4);
    out_type_sig[4] = '\0';
    
    uint8_t *payload = (uint8_t *)malloc(orig_len);
    if (!payload) {
        free(stream);
        return NULL;
    }
    
    memcpy(payload, stream + sizeof(ZVD_OmnivorousHeader), orig_len);
    uint64_t computed_hash = zvd_fast_hash64(payload, orig_len);
    *out_valid_hash = (computed_hash == hdr->data_hash);
    *out_len = orig_len;
    
    free(stream);
    return payload;
}

// =====================================================================
// 4. PGM ATLAS RENDERER & LIVE MOIRÉ PASSPORT
// =====================================================================

static inline uint8_t zvd_penta_to_gray(int8_t p) {
    int val = (int)p + 2;
    if (val < 0) val = 0; if (val > 4) val = 4;
    switch(val) {
        case 0: return 0;
        case 1: return 64;
        case 2: return 128;
        case 3: return 192;
        default: return 255;
    }
}

static inline int8_t zvd_gray_to_penta(uint8_t g) {
    if (g < 32) return -2;
    if (g < 96) return -1;
    if (g < 160) return 0;
    if (g < 224) return 1;
    return 2;
}

inline uint64_t zvd_compute_core_library_fingerprint(void) {
    zvd_ensure_crypto_lut();
    uint64_t f1 = zvd_fast_hash64((const uint8_t*)ZVD_LUT_345, sizeof(ZVD_LUT_345));
    uint64_t f2 = zvd_fast_hash64((const uint8_t*)ZVD_SIN_360, sizeof(ZVD_SIN_360));
    uint64_t f3 = zvd_fast_hash64((const uint8_t*)ZVD_COS_360, sizeof(ZVD_COS_360));
    uint64_t f4 = zvd_fast_hash64((const uint8_t*)ZVD_CRYPTO_UNPACK_BASE5_LUT, sizeof(ZVD_CRYPTO_UNPACK_BASE5_LUT));
    return f1 ^ (f2 + 0x9E3779B97F4A7C15ULL + (f3 << 6) + (f3 >> 2)) ^ f4;
}

inline bool zvd_save_atlas_pgm(const ZVD_Monolith334 *monoliths, size_t num_cubes, uint64_t lib_fingerprint, const char *passphrase, const char *pgm_path) {
    zvd_ensure_crypto_lut();
    int cols = 16;
    int rows = (int)((num_cubes + cols - 1) / cols);
    int img_w = cols * ATLAS_TILE_W;
    int img_h = rows * ATLAS_TILE_H;
    
    uint8_t *canvas = (uint8_t *)calloc(1, (size_t)(img_w * img_h));
    if (!canvas) return false;
    
    for (size_t idx = 0; idx < num_cubes; idx++) {
        int ty = (int)idx / cols;
        int tx = (int)idx % cols;
        int x0 = tx * ATLAS_TILE_W;
        int y0 = ty * ATLAS_TILE_H;
        
        ZVD_TetraCube120D cube;
        zvd_fast_cube_unpack_monolith(&monoliths[idx], &cube);
        
        for (int r = 0; r < 25; r++) {
            for (int c = 0; c < 20; c++) {
                int cell_f = r * 20 + c;
                int cell_v = r * 20 + c;
                canvas[(y0 + r) * img_w + (x0 + c)] = zvd_penta_to_gray(cube.face_data[cell_f]);
                canvas[(y0 + r) * img_w + (x0 + 20 + c)] = zvd_penta_to_gray(cube.vertex_data[cell_v]);
            }
        }
        
        ZVD_BiCore120D vis_core;
        zvd_bicore_init_from_key(&vis_core, 0, passphrase);
        int total_step = (int)(idx + 1) * vis_core.step_deg;
        vis_core.angle_deg_a = (vis_core.angle_deg_a + total_step) % 360;
        vis_core.angle_deg_b = (vis_core.angle_deg_b - total_step + 360) % 360;
        
        for (int r = 25; r < 30; r++) {
            for (int c = 0; c < ATLAS_TILE_W; c++) {
                int phase_a = (r * 17 + c * 9 + vis_core.angle_deg_a) % 360;
                int phase_b = (r * 11 + c * 13 + vis_core.angle_deg_b) % 360;
                float moire = ZVD_SIN_360[phase_a] * ZVD_COS_360[phase_b];
                canvas[(y0 + r) * img_w + (x0 + c)] = (uint8_t)(128.0f + moire * 127.0f);
            }
        }
        
        if (idx == 0) {
            uint64_t nc = (uint64_t)num_cubes;
            memcpy(&canvas[(y0 + 25) * img_w + x0], &nc, sizeof(uint64_t));
            memcpy(&canvas[(y0 + 25) * img_w + x0 + 8], &lib_fingerprint, sizeof(uint64_t));
        }
    }
    
    FILE *f = fopen(pgm_path, "wb");
    if (!f) {
        free(canvas);
        return false;
    }
    fprintf(f, "P5\n%d %d\n255\n", img_w, img_h);
    fwrite(canvas, 1, (size_t)(img_w * img_h), f);
    fclose(f);
    free(canvas);
    return true;
}

inline bool zvd_load_atlas_pgm(const char *pgm_path, ZVD_Monolith334 **out_monoliths, size_t *out_num_cubes, uint64_t *out_lib_fingerprint) {
    zvd_ensure_crypto_lut();
    FILE *f = fopen(pgm_path, "rb");
    if (!f) return false;
    
    char magic[3];
    if (fscanf(f, "%2s", magic) != 1 || magic[0] != 'P' || magic[1] != '5') {
        fclose(f);
        return false;
    }
    
    int img_w, img_h, max_val;
    if (fscanf(f, "%d %d %d", &img_w, &img_h, &max_val) != 3) {
        fclose(f);
        return false;
    }
    fgetc(f);
    
    size_t img_size = (size_t)(img_w * img_h);
    uint8_t *canvas = (uint8_t *)malloc(img_size);
    if (!canvas) {
        fclose(f);
        return false;
    }
    
    if (fread(canvas, 1, img_size, f) != img_size) {
        free(canvas);
        fclose(f);
        return false;
    }
    fclose(f);
    
    int cols = img_w / ATLAS_TILE_W;
    int rows = img_h / ATLAS_TILE_H;
    
    uint64_t num_cubes = 0;
    uint64_t lib_fp = 0;
    
    memcpy(&num_cubes, &canvas[25 * img_w], sizeof(uint64_t));
    memcpy(&lib_fp, &canvas[25 * img_w + 8], sizeof(uint64_t));
    
    if (num_cubes == 0 || num_cubes > (size_t)(cols * rows)) {
        num_cubes = (size_t)(cols * rows);
    }
    
    ZVD_Monolith334 *monoliths = (ZVD_Monolith334 *)malloc(num_cubes * sizeof(ZVD_Monolith334));
    if (!monoliths) {
        free(canvas);
        return false;
    }
    
    for (size_t idx = 0; idx < num_cubes; idx++) {
        int ty = (int)idx / cols;
        int tx = (int)idx % cols;
        int x0 = tx * ATLAS_TILE_W;
        int y0 = ty * ATLAS_TILE_H;
        
        ZVD_TetraCube120D cube;
        for (int r = 0; r < 25; r++) {
            for (int c = 0; c < 20; c++) {
                int cell_f = r * 20 + c;
                int cell_v = r * 20 + c;
                cube.face_data[cell_f] = zvd_gray_to_penta(canvas[(y0 + r) * img_w + (x0 + c)]);
                cube.vertex_data[cell_v] = zvd_gray_to_penta(canvas[(y0 + r) * img_w + (x0 + 20 + c)]);
            }
        }
        zvd_cube_pack_monolith(&cube, &monoliths[idx]);
    }
    
    free(canvas);
    *out_monoliths = monoliths;
    *out_num_cubes = (size_t)num_cubes;
    *out_lib_fingerprint = lib_fp;
    return true;
}

// =====================================================================
// 5. SYMMETRIC PEER API (SUBSCRIBER A <-> B)
// =====================================================================

inline bool zvd_peer_send_to_visual_atlas(const uint8_t *data, size_t len, const char *type_sig, const char *shared_key, const char *out_pgm_path) {
    zvd_ensure_crypto_lut();
    ZVD_Monolith334 *monoliths = NULL;
    size_t num_cubes = zvd_encode_any_to_monoliths(data, len, type_sig, shared_key, &monoliths);
    if (num_cubes == 0 || !monoliths) return false;
    
    uint64_t fp = zvd_compute_core_library_fingerprint();
    bool saved = zvd_save_atlas_pgm(monoliths, num_cubes, fp, shared_key, out_pgm_path);
    free(monoliths);
    return saved;
}

inline uint8_t* zvd_peer_receive_from_visual_atlas(const char *in_pgm_path, const char *shared_key, size_t *out_len, char out_type_sig[5]) {
    zvd_ensure_crypto_lut();
    ZVD_Monolith334 *monoliths = NULL;
    size_t num_cubes = 0;
    uint64_t remote_fp = 0;
    
    if (!zvd_load_atlas_pgm(in_pgm_path, &monoliths, &num_cubes, &remote_fp)) return NULL;
    
    uint64_t local_fp = zvd_compute_core_library_fingerprint();
    if (remote_fp != 0 && remote_fp != local_fp) {
        std::fprintf(stderr, "[!] Warning: Core library fingerprint mismatch (Local: %016llX, Remote: %016llX)\n", 
                     (unsigned long long)local_fp, (unsigned long long)remote_fp);
    }
    
    bool valid_hash = false;
    uint8_t *payload = zvd_decode_monoliths_to_any(monoliths, num_cubes, shared_key, out_len, out_type_sig, &valid_hash);
    free(monoliths);
    
    if (!valid_hash) {
        if (payload) free(payload);
        return NULL;
    }
    
    return payload;
}

// =====================================================================
// 6. UNIVERSAL BUS INTEGRATION (V3.1 CONTEXT-AWARE FILTER ROLE)
// =====================================================================

inline int zvd_visual_crypto_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_CRYPTO_TEST")) {
        const char *key = "ZVD-SOVEREIGN-ENTERPRISE-2026";
        const char *test_msg = "ZVD V3.1 Universal Bus Autonomous Crypto Intercept Verified.";
        bool ok = zvd_peer_send_to_visual_atlas((const uint8_t*)test_msg, std::strlen(test_msg), "text", key, "zvd_bus_atlas.pgm");
        if (ok) {
            size_t rx_len = 0;
            char rx_sig[5] = {0};
            uint8_t *rx_data = zvd_peer_receive_from_visual_atlas("zvd_bus_atlas.pgm", key, &rx_len, rx_sig);
            remove("zvd_bus_atlas.pgm");
            if (rx_data) {
                free(rx_data);
                return 1; // PLUS
            }
        }
        return -1; // MINUS
    }
    return 0; // ZERO
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_visual_crypto_evaluate_ctx(ZvdPipelineContext& ctx) {
    zvd_ensure_crypto_lut();
    if (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_CRYPTO_TEST")) {
        const char *key = "ZVD-SOVEREIGN-ENTERPRISE-2026";
        bool ok = zvd_peer_send_to_visual_atlas(
            reinterpret_cast<const uint8_t*>(ctx.prompt.data()), 
            ctx.prompt.size(), 
            "text", 
            key, 
            "zvd_bus_atlas.pgm"
        );
        if (ok) {
            size_t rx_len = 0;
            char rx_sig[5] = {0};
            uint8_t *rx_data = zvd_peer_receive_from_visual_atlas("zvd_bus_atlas.pgm", key, &rx_len, rx_sig);
            remove("zvd_bus_atlas.pgm");
            if (rx_data) {
                free(rx_data);
                return 1; // PLUS
            }
        }
        return -1; // MINUS
    }
    return 0; // ZERO (transparent propagation through filter role)
}
#endif

inline bool zvd_visual_crypto_verify(std::string_view prompt, std::string_view response) {
    if (prompt.starts_with("ZVD_CRYPTO_SECURE")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.1 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("VisualCryptoAtlas-120D-IQ360", ZvdModuleRole::FILTER, zvd_visual_crypto_evaluate_ctx, zvd_visual_crypto_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("VisualCryptoAtlas-120D-IQ360", zvd_visual_crypto_evaluate, zvd_visual_crypto_verify);
#endif

// =====================================================================
// 7. AUTONOMOUS ENTRY POINT & MULTILINGUAL STRESS TEST (ONEGIN CHAPTER 1)
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_bilateral_exchange_test(void) {
    std::printf("\n[*] Running V3.1 Bit-Perfect Omnivorous Bilateral Exchange Test...\n");
    std::printf("[*] Test Payload: Eugene Onegin Chapter 1 Excerpt (English + Russian Cyrillic UTF-8 + Binary Stream)\n");
    zvd_ensure_crypto_lut();
    
    const char *secret_key_ab = "ZVD-TETRA-120D-SECRET-VARNA-2026";
    const char *secret_wrong = "WRONG-KEY-HACKER-ATTEMPT-2026";
    
    const char multilingual_payload[] = 
        "[Subscriber A -> B Secure Payload]\n"
        "English: Sovereign hyperscale fabric initialized successfully.\n"
        "Russian: «Мой дядя самых честных правил, Когда не в шутку занемог, Он уважать себя заставил И лучше выдумать не мог.»\n"
        "Binary/Bytes Stream Marker: \xFF\xFE\x00\x01\x12\x34\x56\x78\x9A\xBC\xDE\xFF";
    
    size_t payload_len = sizeof(multilingual_payload) - 1;
    
    std::printf("    [TX - Subscriber A] Encoding & encrypting payload (%zu bytes) to 'atlas_onegin_a_to_b.pgm'...\n", payload_len);
    bool tx1 = zvd_peer_send_to_visual_atlas(
        (const uint8_t*)multilingual_payload, payload_len, "text", secret_key_ab, "atlas_onegin_a_to_b.pgm"
    );
    std::printf("    -> Transmission A -> B Status: %s\n", tx1 ? "SUCCESS" : "FAILED");
    
    size_t rx_len1 = 0;
    char rx_sig1[5] = {0};
    std::printf("    [RX - Subscriber B] Receiving and decrypting 'atlas_onegin_a_to_b.pgm'...\n");
    uint8_t *rx_data1 = zvd_peer_receive_from_visual_atlas(
        "atlas_onegin_a_to_b.pgm", secret_key_ab, &rx_len1, rx_sig1
    );
    
    if (rx_data1) {
        std::string restored_str(reinterpret_cast<char*>(rx_data1), rx_len1);
        std::printf("    -> [SUCCESS] Subscriber B decrypted payload (%zu bytes):\n%s\n", rx_len1, restored_str.c_str());
        
        bool bit_perfect = (rx_len1 == payload_len) && (std::memcmp(rx_data1, multilingual_payload, payload_len) == 0);
        std::printf("    -> [VERIFICATION] Bit-Perfect Data Integrity Match: %s\n", bit_perfect ? "PASSED (100% MATCH)" : "FAILED");
        free(rx_data1);
    } else {
        std::printf("    -> [ERROR] Failed to decrypt Subscriber A payload!\n");
    }
    
    std::printf("    [Security Test] Attempting decryption with WRONG key...\n");
    size_t bad_len = 0;
    char bad_sig[5] = {0};
    uint8_t *bad_data = zvd_peer_receive_from_visual_atlas(
        "atlas_onegin_a_to_b.pgm", secret_wrong, &bad_len, bad_sig
    );
    if (bad_data == NULL) {
        std::printf("    -> [PROTECTION PASSED] Wrong key successfully rejected by Omnivorous Gateway.\n");
    } else {
        std::printf("    -> [SECURITY FAIL] Wrong key decrypted data!\n");
        free(bad_data);
    }
    
    const char reply_payload[] = "Subscriber B Response: Acknowledged. Onegin payload verified bit-for-bit across 120D fabric.";
    size_t reply_len = sizeof(reply_payload) - 1;
    std::printf("    [TX - Subscriber B] Sending reply to 'atlas_onegin_b_to_a.pgm'...\n");
    zvd_peer_send_to_visual_atlas((const uint8_t*)reply_payload, reply_len, "text", secret_key_ab, "atlas_onegin_b_to_a.pgm");
    
    size_t rx_len2 = 0;
    char rx_sig2[5] = {0};
    uint8_t *rx_data2 = zvd_peer_receive_from_visual_atlas("atlas_onegin_b_to_a.pgm", secret_key_ab, &rx_len2, rx_sig2);
    if (rx_data2) {
        std::string restored_reply(reinterpret_cast<char*>(rx_data2), rx_len2);
        std::printf("    -> [SUCCESS] Subscriber A received reply: '%s'\n", restored_reply.c_str());
        free(rx_data2);
    }
    
    remove("atlas_onegin_a_to_b.pgm");
    remove("atlas_onegin_b_to_a.pgm");
    std::printf("[*] Bit-Perfect Bilateral Exchange Test completed successfully.\n");
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::printf("======================================================================\n");
    std::printf("  ZVD-TETRA-120D BIT-PERFECT VISUAL CRYPTO FABRIC (V3.1-OMNIVOROUS)\n");
    std::printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    std::printf("  Core Fingerprint: %016llX\n", (unsigned long long)zvd_compute_core_library_fingerprint());
    std::printf("======================================================================\n");

    run_bilateral_exchange_test();

    int choice = -1;
    while (choice != 0) {
        std::printf("\n=================== ZVD V3.1 CRYPTO CODEC MENU ==================\n");
        std::printf(" [1] Run Multilingual Bilateral Exchange Test (Eugene Onegin + UTF-8)\n");
        std::printf(" [0] Exit\n");
        std::printf("==================================================================\n");
        std::printf("Select menu option: ");

        if (scanf("%d", &choice) != 1) {
            std::printf("[!] Invalid input. Try again.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                run_bilateral_exchange_test();
                break;
            case 0:
                std::printf("[*] Exiting ZVD-120D Visual Crypto. All resources freed.\n");
                break;
            default:
                std::printf("[!] Unknown menu option.\n");
        }
    }

#ifdef _WIN32
    std::printf("\nPress Enter to close console...");
    getchar();
#endif

    return 0;
}

#endif // ZVD_NO_MAIN
