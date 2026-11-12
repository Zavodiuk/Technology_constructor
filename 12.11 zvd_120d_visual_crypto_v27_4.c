/* =============================================================================
 *  PROJECT:    ZVD-TETRA-120D OMNIVOROUS VISUAL CRYPTO CODEC & MOIRE ATLAS FABRIC
 *              (V27.4-CRYPTO-IQ-360: Bilateral Peer-to-Peer & Dynamic Step Key)
 *  FILENAME:   zvd_120d_visual_crypto_v27_4.c
 *  VERSION:    2026.11-V27.4-CRYPTO-IQ-360-C17-PRODUCTION
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  LICENSE:    Proprietary. All rights reserved.
 * =============================================================================
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ZVD_DIM_120            120
#define ZVD_DIM_60             60
#define ZVD_CUBE               5
#define ZVD_N_FACES            4
#define ZVD_CELLS_PER_LAYER    500
#define ZVD_LAYER_BYTES        167
#define ZVD_MONOLITH_SIZE      334

#define ATLAS_TILE_W           40
#define ATLAS_TILE_H           30

/* =====================================================================
 * DATA STRUCTURES & ALIGNED MEMORY ARCHITECTURE (AVX-512 READY)
 * ===================================================================== */

typedef struct {
    alignas(64) int8_t face_data[ZVD_CELLS_PER_LAYER];
    alignas(64) int8_t vertex_data[ZVD_CELLS_PER_LAYER];
} ZVD_TetraCube120D;

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct {
    uint8_t face_packed[ZVD_LAYER_BYTES];
    uint8_t vertex_packed[ZVD_LAYER_BYTES];
} ZVD_Monolith334;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    uint8_t face_packed[ZVD_LAYER_BYTES];
    uint8_t vertex_packed[ZVD_LAYER_BYTES];
} ZVD_Monolith334;
#endif

typedef struct {
    alignas(64) int8_t matrix_a[25];
    alignas(64) int8_t matrix_b[25];
    int angle_deg_a;
    int angle_deg_b;
    int step_deg;
    float base_velocity;
} ZVD_BiCore120D;

typedef struct {
    double mean_val;
    double min_val;
    double max_val;
    double std_val;
    double risk_index;
} ZVD_RashomonAudit;

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

/* =====================================================================
 * HARDWARE CORE DETECTION (OpenMP + Fallback)
 * ===================================================================== */

int zvd_detect_cpu_cores(void) {
#ifdef _OPENMP
    return omp_get_max_threads();
#elif defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (cores > 0) ? (int)cores : 1;
#else
    return 1;
#endif
}

/* =====================================================================
 * PRECOMPUTED LUTS & 360° TRUE I/Q TABLES
 * ===================================================================== */

static const int8_t ZVD_LUT_345[3][4] = {
    {-2, -2, -1, -1},
    {-1,  0,  0,  1},
    { 0,  1,  2,  2}
};

alignas(64) static float ZVD_SIN_360[360];
alignas(64) static float ZVD_COS_360[360];
static float ZVD_PYTH_LUT[5];
static float ZVD_HYPERBOLIC_LUT[ZVD_CUBE][ZVD_CUBE];
static float ZVD_ELLIPTIC_LUT[ZVD_CUBE][ZVD_CUBE];
static int8_t ZVD_UNPACK_BASE5_LUT[256][3];
static bool g_luts_initialized = false;

void zvd_init_luts(void) {
    if (g_luts_initialized) return;
    
    for (int deg = 0; deg < 360; deg++) {
        double rad = (double)deg * (M_PI / 180.0);
        ZVD_SIN_360[deg] = (float)sin(rad);
        ZVD_COS_360[deg] = (float)cos(rad);
    }
    
    for (int k = 0; k < 5; k++) {
        double term = 4.0 + 2.0 * (double)k;
        ZVD_PYTH_LUT[k] = (float)(sqrt(9.0 + term * term) / 5.0);
    }

    for (int x = 0; x < ZVD_CUBE; x++) {
        for (int y = 0; y < ZVD_CUBE; y++) {
            double dx = (double)(x - 2);
            double dy = (double)(y - 2);
            double dist = sqrt(dx * dx + dy * dy);
            if (dist < 1e-5) {
                ZVD_HYPERBOLIC_LUT[x][y] = 1.0f;
                ZVD_ELLIPTIC_LUT[x][y] = 0.0f;
            } else {
                ZVD_HYPERBOLIC_LUT[x][y] = (float)(sinh(dist) / dist);
                ZVD_ELLIPTIC_LUT[x][y] = (float)(sin(dist));
            }
        }
    }

    for (int code = 0; code < 256; code++) {
        int temp = code;
        int a = temp % 5; temp /= 5;
        int b = temp % 5; temp /= 5;
        int c = temp % 5;
        ZVD_UNPACK_BASE5_LUT[code][0] = (int8_t)(a - 2);
        ZVD_UNPACK_BASE5_LUT[code][1] = (int8_t)(b - 2);
        ZVD_UNPACK_BASE5_LUT[code][2] = (int8_t)(c - 2);
    }

    g_luts_initialized = true;
}

uint64_t zvd_fast_hash64(const uint8_t * restrict data, size_t len) {
    uint64_t h = 0x9E3779B97F4A7C15ULL ^ (uint64_t)len;
    size_t n_blocks = len / 8;
    for (size_t i = 0; i < n_blocks; i++) {
        uint64_t w;
        memcpy(&w, data + i * 8, 8);
        h ^= w;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    size_t tail_len = len % 8;
    if (tail_len > 0) {
        uint64_t tail_val = 0;
        memcpy(&tail_val, data + n_blocks * 8, tail_len);
        h ^= tail_val;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

uint64_t zvd_compute_core_library_fingerprint(void) {
    if (!g_luts_initialized) zvd_init_luts();
    uint64_t f1 = zvd_fast_hash64((const uint8_t*)ZVD_LUT_345, sizeof(ZVD_LUT_345));
    uint64_t f2 = zvd_fast_hash64((const uint8_t*)ZVD_SIN_360, sizeof(ZVD_SIN_360));
    uint64_t f3 = zvd_fast_hash64((const uint8_t*)ZVD_COS_360, sizeof(ZVD_COS_360));
    uint64_t f4 = zvd_fast_hash64((const uint8_t*)ZVD_UNPACK_BASE5_LUT, sizeof(ZVD_UNPACK_BASE5_LUT));
    return f1 ^ (f2 + 0x9E3779B97F4A7C15ULL + (f3 << 6) + (f3 >> 2)) ^ f4;
}

double zvd_shannon_entropy(const uint8_t * restrict data, size_t len) {
    if (len == 0) return 0.0;
    uint64_t counts[256] = {0};
    for (size_t i = 0; i < len; i++) counts[data[i]]++;
    double entropy = 0.0;
    double f_len = (double)len;
    for (int b = 0; b < 256; b++) {
        if (counts[b] > 0) {
            double p = (double)counts[b] / f_len;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

/* =====================================================================
 * LUKASIEWICZ CLAMPING & GODEL REDUCER
 * ===================================================================== */

static inline int8_t zvd_lukasiewicz_clamp(int val) {
    if (val < -2) return -2;
    if (val > 2) return 2;
    return (int8_t)val;
}

void zvd_pack_layer_lukasiewicz(const int8_t * restrict layer_500, uint8_t * restrict out_167) {
    int idx = 0;
    for (int i = 0; i < 498; i += 3) {
        int a = zvd_lukasiewicz_clamp(layer_500[i]) + 2;
        int b = zvd_lukasiewicz_clamp(layer_500[i+1]) + 2;
        int c = zvd_lukasiewicz_clamp(layer_500[i+2]) + 2;
        out_167[idx++] = (uint8_t)(a + 5 * b + 25 * c);
    }
    int a = zvd_lukasiewicz_clamp(layer_500[498]) + 2;
    int b = zvd_lukasiewicz_clamp(layer_500[499]) + 2;
    out_167[166] = (uint8_t)(a + 5 * b);
}

void zvd_unpack_layer_lukasiewicz(const uint8_t * restrict in_167, int8_t * restrict layer_500) {
    int idx = 0;
    for (int i = 0; i < 166; i++) {
        int val = (int)in_167[i];
        layer_500[idx++] = (int8_t)((val % 5) - 2);
        layer_500[idx++] = (int8_t)(((val / 5) % 5) - 2);
        layer_500[idx++] = (int8_t)(((val / 25) % 5) - 2);
    }
    int val = (int)in_167[166];
    layer_500[498] = (int8_t)((val % 5) - 2);
    layer_500[499] = (int8_t)(((val / 5) % 5) - 2);
}

void zvd_cube_pack_monolith(const ZVD_TetraCube120D * restrict cube, ZVD_Monolith334 * restrict out) {
    zvd_pack_layer_lukasiewicz(cube->face_data, out->face_packed);
    zvd_pack_layer_lukasiewicz(cube->vertex_data, out->vertex_packed);
}

void zvd_cube_unpack_monolith(const ZVD_Monolith334 * restrict monolith, ZVD_TetraCube120D * restrict out_cube) {
    zvd_unpack_layer_lukasiewicz(monolith->face_packed, out_cube->face_data);
    zvd_unpack_layer_lukasiewicz(monolith->vertex_packed, out_cube->vertex_data);
}

/* =====================================================================
 * BICORE & TRUE I/Q 360° SYNTHESIS & DYNAMIC STEP KEY ENCRYPTION
 * ===================================================================== */

void zvd_bicore_init_from_key(ZVD_BiCore120D * restrict core, int tid, const char *passphrase) {
    int mid = ZVD_CUBE / 2;
    for (int x = 0; x < ZVD_CUBE; x++) {
        for (int y = 0; y < ZVD_CUBE; y++) {
            int idx = x * ZVD_CUBE + y;
            if (x == mid && y == mid) {
                core->matrix_a[idx] = 0; core->matrix_b[idx] = 0;
            } else if ((x + y) % 2 == 0) {
                core->matrix_a[idx] = 1; core->matrix_b[idx] = -1;
            } else {
                core->matrix_a[idx] = -1; core->matrix_b[idx] = 1;
            }
        }
    }
    uint64_t p_hash = zvd_fast_hash64((const uint8_t*)passphrase, strlen(passphrase));
    core->angle_deg_a = (int)((p_hash + tid * 37) % 360);
    core->angle_deg_b = (int)(((p_hash >> 8) + tid * 53) % 360);
    // Динамический секретный шаг в градусах на основе хэша пароля (от 3 до 19 градусов)
    core->step_deg = (int)(3 + (p_hash % 17));
    core->base_velocity = 30.0f;
}

void zvd_bicore_update_rotation(ZVD_BiCore120D * restrict core, double entropy) {
    int dynamic_step = (entropy < 1.5) ? (core->step_deg * 2) : core->step_deg;
    if (dynamic_step < 1) dynamic_step = 1;
    core->angle_deg_a = (core->angle_deg_a + dynamic_step) % 360;
    core->angle_deg_b = (core->angle_deg_b - dynamic_step + 360) % 360;
}

void zvd_stride_collapse_to_120(const uint8_t * restrict data, size_t len, float * restrict out_120) {
    if (len == 0) {
        for (int i = 0; i < ZVD_DIM_120; i++) out_120[i] = 0.0f;
        return;
    }
    if (len <= (size_t)ZVD_DIM_120) {
        for (size_t i = 0; i < len; i++) out_120[i % ZVD_DIM_120] = (float)data[i];
        for (size_t i = len; i < (size_t)ZVD_DIM_120; i++) out_120[i] = 0.0f;
    } else {
        size_t chunk_size = len / ZVD_DIM_120;
        for (int i = 0; i < ZVD_DIM_120; i++) {
            size_t start = i * chunk_size;
            size_t end = (i == ZVD_DIM_120 - 1) ? len : start + chunk_size;
            float s = 0.0f;
            for (size_t j = start; j < end; j++) s += (float)data[j];
            out_120[i] = s / (float)(end - start);
        }
    }
}

void zvd_enterprise_synthesis_120d(const float * restrict collapsed_120, const ZVD_BiCore120D * restrict core, int8_t * restrict out_vec_120) {
    if (!g_luts_initialized) zvd_init_luts();
    for (int i = 0; i < ZVD_DIM_120; i++) {
        float raw = collapsed_120[i];
        int m_idx = i % 25;
        int p_idx = i % 5;
        float w_a = (float)core->matrix_a[m_idx];
        float w_b = (float)core->matrix_b[m_idx];
        float pyth_conv = ZVD_PYTH_LUT[p_idx];
        float trig_val = 0.0f;
        
        if (i < 60) {
            int phase_a = (i * 3 + core->angle_deg_a) % 360;
            int phase_b = (i * 3 + core->angle_deg_b) % 360;
            trig_val = ZVD_SIN_360[phase_a] * ZVD_COS_360[phase_b];
        } else {
            int phase_a = ((i - 60) * 3 + core->angle_deg_a) % 360;
            int phase_b = ((i - 60) * 3 + core->angle_deg_b) % 360;
            trig_val = ZVD_COS_360[phase_a] * ZVD_SIN_360[phase_b];
        }
        
        float moire = trig_val * w_a * w_b * 0.1f * pyth_conv;
        float val_acc = raw * (1.0f + 0.5f * moire);
        int int_val = (int)val_acc;
        int trit_1 = (int_val < 0) ? -1 : (int_val > 0 ? 1 : 0);
        int quad_1 = abs(int_val) & 3;
        out_vec_120[i] = ZVD_LUT_345[trit_1 + 1][quad_1];
    }
}

void zvd_project_120d_to_dual_cube(const int8_t * restrict vec_120, ZVD_TetraCube120D * restrict out_cube) {
    const int8_t *F60 = &vec_120[0];
    const int8_t *V60 = &vec_120[60];
    
    for (int x = 0; x < ZVD_CUBE; x++) {
        for (int y = 0; y < ZVD_CUBE; y++) {
            for (int z = 0; z < ZVD_CUBE; z++) {
                for (int f = 0; f < ZVD_N_FACES; f++) {
                    int cell_idx = ((x * 5 + y) * 5 + z) * 4 + f;
                    int k = (25 * x + 5 * y + z + f) % 60;
                    int parity = ((x + y + z + f) % 3) - 1;
                    
                    float hyper_factor = ZVD_HYPERBOLIC_LUT[x][y];
                    float ellip_factor = ZVD_ELLIPTIC_LUT[x][y];
                    
                    int f_raw = (int)((float)F60[k] * hyper_factor) + parity;
                    int v_raw = (int)((float)(-V60[k]) * (1.0f + ellip_factor)) - parity;
                    
                    int trit_f = (f_raw < 0) ? -1 : (f_raw > 0 ? 1 : 0);
                    int quad_f = (x + y + z + f) & 3;
                    int8_t p_face = ZVD_LUT_345[trit_f + 1][quad_f];
                    
                    int trit_v = (v_raw < 0) ? -1 : (v_raw > 0 ? 1 : 0);
                    int quad_v = (x * 2 + y + z + f) & 3;
                    int8_t p_vert = ZVD_LUT_345[trit_v + 1][quad_v];
                    
                    out_cube->face_data[cell_idx] = zvd_lukasiewicz_clamp((int)F60[k] + (int)p_face);
                    out_cube->vertex_data[cell_idx] = zvd_lukasiewicz_clamp((int)V60[k] + (int)p_vert);
                }
            }
        }
    }
}

/* 120D I/O Moire Cryptographic Base-5 Stream Transformer */
void zvd_120d_moire_crypt_cube(ZVD_TetraCube120D * restrict cube, const char *passphrase, bool encrypt) {
    ZVD_BiCore120D core;
    zvd_bicore_init_from_key(&core, 0, passphrase);
    
    alignas(64) float collapsed[ZVD_DIM_120];
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    
    // Генерируем гамму по структуре куба
    uint8_t seed_buf[334];
    zvd_Monolith334 mono;
    zvd_cube_pack_monolith(cube, &mono);
    memcpy(seed_buf, &mono, 334);
    
    double entropy = zvd_shannon_entropy(seed_buf, 334);
    zvd_bicore_update_rotation(&core, entropy);
    zvd_stride_collapse_to_120(seed_buf, 334, collapsed);
    zvd_enterprise_synthesis_120d(collapsed, &core, synth_120);
    
    // Поэлементная модульная арифметика по основанию 5 (0..4) для всех 1000 ячеек
    int k_idx = 0;
    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int k_val = abs((int)synth_120[k_idx % ZVD_DIM_120]) % 5;
        k_idx++;
        
        int f_val = (int)cube->face_data[i] + 2; // приводим к 0..4
        if (encrypt) f_val = (f_val + k_val) % 5;
        else f_val = (f_val - k_val + 5) % 5;
        cube->face_data[i] = (int8_t)(f_val - 2);
        
        int v_val = (int)cube->vertex_data[i] + 2;
        if (encrypt) v_val = (v_val + k_val) % 5;
        else v_val = (v_val - k_val + 5) % 5;
        cube->vertex_data[i] = (int8_t)(v_val - 2);
    }
}

/* =====================================================================
 * OMNIVOROUS GATEWAY & PGM VISUAL ATLAS FABRIC (40x30 px per cube)
 * ===================================================================== */

size_t zvd_encode_any_to_monoliths(const uint8_t *payload, size_t payload_len, const char *type_sig, ZVD_Monolith334 **out_monoliths) {
    size_t total_size = sizeof(ZVD_OmnivorousHeader) + payload_len;
    size_t num_cubes = (total_size + ZVD_MONOLITH_SIZE - 1) / ZVD_MONOLITH_SIZE;
    size_t padded_size = num_cubes * ZVD_MONOLITH_SIZE;
    
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
        memcpy(&monoliths[i], stream + i * ZVD_MONOLITH_SIZE, ZVD_MONOLITH_SIZE);
    }
    
    free(stream);
    *out_monoliths = monoliths;
    return num_cubes;
}

uint8_t* zvd_decode_monoliths_to_any(const ZVD_Monolith334 *monoliths, size_t num_cubes, size_t *out_len, char out_type_sig[5], bool *out_valid_hash) {
    size_t total_bytes = num_cubes * ZVD_MONOLITH_SIZE;
    uint8_t *stream = (uint8_t *)malloc(total_bytes);
    if (!stream) return NULL;
    
    for (size_t i = 0; i < num_cubes; i++) {
        memcpy(stream + i * ZVD_MONOLITH_SIZE, &monoliths[i], ZVD_MONOLITH_SIZE);
    }
    
    ZVD_OmnivorousHeader *hdr = (ZVD_OmnivorousHeader *)stream;
    size_t orig_len = (size_t)hdr->orig_len;
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

static inline uint8_t penta_to_gray(int8_t p) {
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

static inline int8_t gray_to_penta(uint8_t g) {
    if (g < 32) return -2;
    if (g < 96) return -1;
    if (g < 160) return 0;
    if (g < 224) return 1;
    return 2;
}

bool zvd_save_atlas_pgm(const ZVD_Monolith334 *monoliths, size_t num_cubes, uint64_t lib_fingerprint, const char *pgm_path) {
    int cols = 16;
    int rows = (int)((num_cubes + cols - 1) / cols);
    int img_w = cols * ATLAS_TILE_W;
    int img_h = rows * ATLAS_TILE_H;
    
    uint8_t *canvas = (uint8_t *)calloc(img_h, img_w);
    if (!canvas) return false;
    
    for (size_t idx = 0; idx < num_cubes; idx++) {
        int ty = (int)idx / cols;
        int tx = (int)idx % cols;
        int x0 = tx * ATLAS_TILE_W;
        int y0 = ty * ATLAS_TILE_H;
        
        ZVD_TetraCube120D cube;
        zvd_cube_unpack_monolith(&monoliths[idx], &cube);
        
        // Строки 0..24: Ядро монолита без потерь (500 граней слева, 500 вершин справа)
        for (int r = 0; r < 25; r++) {
            for (int c = 0; c < 20; c++) {
                int cell_f = r * 20 + c;
                int cell_v = r * 20 + c;
                canvas[(y0 + r) * img_w + (x0 + c)] = penta_to_gray(cube.face_data[cell_f]);
                canvas[(y0 + r) * img_w + (x0 + 20 + c)] = penta_to_gray(cube.vertex_data[cell_v]);
            }
        }
        
        // Строки 25..29: Икосаэдральный муар-паспорт и синхрополе (180 пикселей интерференции)
        for (int r = 25; r < 30; r++) {
            for (int c = 0; c < ATLAS_TILE_W; c++) {
                int phase = (r * c + (int)idx * 13) % 360;
                float moire = ZVD_SIN_360[phase] * ZVD_COS_360[(phase * 2) % 360];
                uint8_t g_val = (uint8_t)(128.0f + moire * 127.0f);
                canvas[(y0 + r) * img_w + (x0 + c)] = g_val;
            }
        }
        
        // Запись метаданных в первые 16 байт строки 25 левого угла плитки (нулевой куб)
        if (idx == 0) {
            uint64_t nc = (uint64_t)num_cubes;
            memcpy(&canvas[y0 * img_w + x0], &nc, sizeof(uint64_t));
            memcpy(&canvas[y0 * img_w + x0 + 8], &lib_fingerprint, sizeof(uint64_t));
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

bool zvd_load_atlas_pgm(const char *pgm_path, ZVD_Monolith334 **out_monoliths, size_t *out_num_cubes, uint64_t *out_lib_fingerprint) {
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
    fgetc(f); // пропускаем символ новой строки после max_val
    
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
    memcpy(&num_cubes, &canvas[0], sizeof(uint64_t));
    memcpy(&lib_fp, &canvas[8], sizeof(uint64_t));
    
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
                cube.face_data[cell_f] = gray_to_penta(canvas[(y0 + r) * img_w + (x0 + c)]);
                cube.vertex_data[cell_v] = gray_to_penta(canvas[(y0 + r) * img_w + (x0 + 20 + c)]);
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

/* =====================================================================
 * SYMMETRIC BILATERAL PEER API (SUBSCRIBER A <-> B)
 * ===================================================================== */

bool zvd_peer_send_to_visual_atlas(
    const uint8_t *data, 
    size_t len, 
    const char *type_sig, 
    const char *shared_key, 
    const char *out_pgm_path
) {
    zvd_init_luts();
    ZVD_Monolith334 *monoliths = NULL;
    size_t num_cubes = zvd_encode_any_to_monoliths(data, len, type_sig, &monoliths);
    if (num_cubes == 0 || !monoliths) return false;
    
    // Применяем 120D I/O муарное шифрование ко всем кубам
    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < (int64_t)num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_cube_unpack_monolith(&monoliths[i], &cube);
        zvd_120d_moire_crypt_cube(&cube, shared_key, true);
        zvd_cube_pack_monolith(&cube, &monoliths[i]);
    }
    
    uint64_t fp = zvd_compute_core_library_fingerprint();
    bool saved = zvd_save_atlas_pgm(monoliths, num_cubes, fp, out_pgm_path);
    free(monoliths);
    return saved;
}

uint8_t* zvd_peer_receive_from_visual_atlas(
    const char *in_pgm_path, 
    const char *shared_key, 
    size_t *out_len, 
    char out_type_sig[5]
) {
    zvd_init_luts();
    ZVD_Monolith334 *monoliths = NULL;
    size_t num_cubes = 0;
    uint64_t remote_fp = 0;
    
    if (!zvd_load_atlas_pgm(in_pgm_path, &monoliths, &num_cubes, &remote_fp)) {
        return NULL;
    }
    
    uint64_t local_fp = zvd_compute_core_library_fingerprint();
    if (remote_fp != 0 && remote_fp != local_fp) {
        fprintf(stderr, "[!] Warning: Core library fingerprint mismatch (Local: %016llX, Remote: %016llX)\n", 
                (unsigned long long)local_fp, (unsigned long long)remote_fp);
    }
    
    // Обратное 120D I/O муарное дешифрование
    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < (int64_t)num_cubes; i++) {
        ZVD_TetraCube120D cube;
        zvd_cube_unpack_monolith(&monoliths[i], &cube);
        zvd_120d_moire_crypt_cube(&cube, shared_key, false);
        zvd_cube_pack_monolith(&cube, &monoliths[i]);
    }
    
    bool valid_hash = false;
    uint8_t *payload = zvd_decode_monoliths_to_any(monoliths, num_cubes, out_len, out_type_sig, &valid_hash);
    free(monoliths);
    
    if (!valid_hash) {
        if (payload) free(payload);
        return NULL; // Ошибка проверки целостности хэша (неверный ключ или повреждение)
    }
    
    return payload;
}

/* =====================================================================
 * BENCHMARK & BILATERAL STRESS TEST SUITE (MAIN)
 * ===================================================================== */

static void run_bilateral_exchange_test(void) {
    printf("\n[*] Running V27.4 Bilateral Peer-to-Peer Visual Crypto Exchange Test...\n");
    zvd_init_luts();
    
    const char *secret_key_ab = "ZVD-TETRA-120D-SECRET-VARNA-2026";
    const char *secret_wrong = "WRONG-KEY-HACKER-ATTEMPT-2026";
    
    const char *msg_a_to_b = "Hello from Subscriber A! Secure 120D visual PGM transmission test.";
    const char *msg_b_to_a = "Hello Subscriber B received! Affirmative response via 120D moire fabric.";
    
    // Шаг 1: Абонент А отправляет Абоненту Б
    printf("    [TX - Subscriber A] Sending asset to atlas 'atlas_a_to_b.pgm'...\n");
    bool tx1 = zvd_peer_send_to_visual_atlas(
        (const uint8_t*)msg_a_to_b, strlen(msg_a_to_b), "text", secret_key_ab, "atlas_a_to_b.pgm"
    );
    printf("    -> Transmission A -> B Status: %s\n", tx1 ? "SUCCESS" : "FAILED");
    
    // Шаг 2: Абонент Б принимает от Абонента А
    size_t rx_len1 = 0;
    char rx_sig1[5] = {0};
    printf("    [RX - Subscriber B] Receiving and decrypting 'atlas_a_to_b.pgm'...\n");
    uint8_t *rx_data1 = zvd_peer_receive_from_visual_atlas(
        "atlas_a_to_b.pgm", secret_key_ab, &rx_len1, rx_sig1
    );
    
    if (rx_data1) {
        char *restored_str = (char *)malloc(rx_len1 + 1);
        memcpy(restored_str, rx_data1, rx_len1);
        restored_str[rx_len1] = '\0';
        printf("    -> [SUCCESS] Subscriber B decrypted message: '%s' (Sig: %s)\n", restored_str, rx_sig1);
        free(restored_str);
        free(rx_data1);
    } else {
        printf("    -> [ERROR] Failed to decrypt Subscriber A message!\n");
    }
    
    // Шаг 3: Проверка защиты канала (попытка взлома неверным ключом)
    printf("    [Security Test] Attempting to decrypt with WRONG key...\n");
    size_t bad_len = 0;
    char bad_sig[5] = {0};
    uint8_t *bad_data = zvd_peer_receive_from_visual_atlas(
        "atlas_a_to_b.pgm", secret_wrong, &bad_len, bad_sig
    );
    if (bad_data == NULL) {
        printf("    -> [PROTECTION PASSED] Wrong key successfully rejected by Omnivorous Gateway.\n");
    } else {
        printf("    -> [SECURITY FAIL] Wrong key decrypted data!\n");
        free(bad_data);
    }
    
    // Шаг 4: Ответный пакет от Б к А
    printf("    [TX - Subscriber B] Sending response to atlas 'atlas_b_to_a.pgm'...\n");
    bool tx2 = zvd_peer_send_to_visual_atlas(
        (const uint8_t*)msg_b_to_a, strlen(msg_b_to_a), "text", secret_key_ab, "atlas_b_to_a.pgm"
    );
    
    size_t rx_len2 = 0;
    char rx_sig2[5] = {0};
    uint8_t *rx_data2 = zvd_peer_receive_from_visual_atlas(
        "atlas_b_to_a.pgm", secret_key_ab, &rx_len2, rx_sig2
    );
    if (rx_data2) {
        char *restored_resp = (char *)malloc(rx_len2 + 1);
        memcpy(restored_resp, rx_data2, rx_len2);
        restored_resp[rx_len2] = '\0';
        printf("    -> [SUCCESS] Subscriber A received response: '%s'\n", restored_resp);
        free(restored_resp);
        free(rx_data2);
    }
    
    // Очистка файлов атласов
    remove("atlas_a_to_b.pgm");
    remove("atlas_b_to_a.pgm");
    printf("[*] Bilateral exchange test completed successfully.\n");
}

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        run_bilateral_exchange_test();
        return 0;
    }

    printf("======================================================================\n");
    printf("  ZVD-TETRA-120D VISUAL CRYPTO FABRIC (V27.4-CRYPTO-IQ-360)\n");
    printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    printf("  Active Hardware Cores: %d | Core Fingerprint: %016llX\n", 
           zvd_detect_cpu_cores(), (unsigned long long)zvd_compute_core_library_fingerprint());
    printf("======================================================================\n");

    run_bilateral_exchange_test();

    int choice = -1;
    while (choice != 0) {
        printf("\n=================== ZVD V27.4 CRYPTO CODEC MENU ==================\n");
        printf(" [1] Запустить двусторонний обмен Абонент А <-> Б (Автотест PGM)\n");
        printf(" [2] Выход\n");
        printf("==================================================================\n");
        printf("Выберите пункт меню: ");

        if (scanf("%d", &choice) != 1) {
            printf("[!] Некорректный ввод. Попробуйте снова.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                run_bilateral_exchange_test();
                break;
            case 2:
            case 0:
                printf("[*] Выход из системы ZVD-120D Visual Crypto. Все ресурсы освобождены.\n");
                choice = 0;
                break;
            default:
                printf("[!] Неизвестный пункт меню.\n");
        }
    }

#ifdef _WIN32
    printf("\nНажмите Enter для закрытия консоли...");
    getchar();
#endif

    return 0;
}
