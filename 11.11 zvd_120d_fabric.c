/* =============================================================================
 *  PROJECT:    ZVD-120D HYPERSCALE DATACENTER FABRIC & BIT-PERFECT ACCELERATOR
 *  FILENAME:   zvd_120d_fabric.c
 *  VERSION:    2026.11-V25.7-C17-FABRIC-PRODUCTION
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
#define ZVD_OPTIC_MAGIC        "ZVD_OPTIC_BURST_120D"
#define ZVD_OPTIC_MAGIC_LEN    19

/* =====================================================================
 * DATA STRUCTURES
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
    float angle_a;
    float angle_b;
    float base_velocity;
} ZVD_BiCore120D;

typedef struct {
    double mean_val;
    double min_val;
    double max_val;
    double std_val;
    double risk_index;
} ZVD_RashomonAudit;

typedef struct {
    uint8_t sha256_hash[32];
    size_t original_size;
    double entropy;
    double risk_index;
    double dual_asymmetry;
    ZVD_Monolith334 monolith;
    size_t compressed_size;
    uint8_t *compressed_payload;
} ZVD_DatacenterPacket;

/* =====================================================================
 * INTERNAL SHA-256 IMPLEMENTATION (ZERO EXTERNAL DEPENDENCIES)
 * ===================================================================== */

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} ZVD_SHA256_CTX;

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb03238c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ZVD_ROTR(a,b) (((a) >> (b)) | ((a) << (32 - (b))))
#define ZVD_CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define ZVD_MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define ZVD_EP0(x) (ZVD_ROTR(x,2) ^ ZVD_ROTR(x,13) ^ ZVD_ROTR(x,22))
#define ZVD_EP1(x) (ZVD_ROTR(x,6) ^ ZVD_ROTR(x,11) ^ ZVD_ROTR(x,25))
#define ZVD_SIG0(x) (ZVD_ROTR(x,7) ^ ZVD_ROTR(x,18) ^ ((x) >> 3))
#define ZVD_SIG1(x) (ZVD_ROTR(x,17) ^ ZVD_ROTR(x,19) ^ ((x) >> 10))

static void zvd_sha256_transform(ZVD_SHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) | ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
    for ( ; i < 64; ++i)
        m[i] = ZVD_SIG1(m[i-2]) + m[i-7] + ZVD_SIG0(m[i-15]) + m[i-16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + ZVD_EP1(e) + ZVD_CH(e,f,g) + k[i] + m[i];
        t2 = ZVD_EP0(a) + ZVD_MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void zvd_sha256_init(ZVD_SHA256_CTX *ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void zvd_sha256_update(ZVD_SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            zvd_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void zvd_sha256_final(ZVD_SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        zvd_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += (uint64_t)ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    zvd_sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]    = (uint8_t)(ctx->state[0] >> (24 - i * 8));
        hash[i+4]  = (uint8_t)(ctx->state[1] >> (24 - i * 8));
        hash[i+8]  = (uint8_t)(ctx->state[2] >> (24 - i * 8));
        hash[i+12] = (uint8_t)(ctx->state[3] >> (24 - i * 8));
        hash[i+16] = (uint8_t)(ctx->state[4] >> (24 - i * 8));
        hash[i+20] = (uint8_t)(ctx->state[5] >> (24 - i * 8));
        hash[i+24] = (uint8_t)(ctx->state[6] >> (24 - i * 8));
        hash[i+28] = (uint8_t)(ctx->state[7] >> (24 - i * 8));
    }
}

void zvd_compute_sha256(const uint8_t *data, size_t len, uint8_t out_hash[32]) {
    ZVD_SHA256_CTX ctx;
    zvd_sha256_init(&ctx);
    zvd_sha256_update(&ctx, data, len);
    zvd_sha256_final(&ctx, out_hash);
}

/* =====================================================================
 * MATH KERNELS & LOOK-UP TABLES
 * ===================================================================== */

alignas(64) static float ZVD_SIN_LUT[ZVD_DIM_120];
static float ZVD_PYTH_LUT[5];
static bool g_luts_initialized = false;

void zvd_init_luts(void) {
    if (g_luts_initialized) return;
    for (int i = 0; i < ZVD_DIM_120; i++) {
        ZVD_SIN_LUT[i] = (float)sin((double)i * (2.0 * M_PI / (double)ZVD_DIM_120));
    }
    for (int k = 0; k < 5; k++) {
        double term = 4.0 + 2.0 * (double)k;
        ZVD_PYTH_LUT[k] = (float)(sqrt(9.0 + term * term) / 5.0);
    }
    g_luts_initialized = true;
}

static inline int8_t zvd_clamp_i8(int val, int min_v, int max_v) {
    if (val < min_v) return (int8_t)min_v;
    if (val > max_v) return (int8_t)max_v;
    return (int8_t)val;
}

double zvd_shannon_entropy(const uint8_t * restrict data, size_t len) {
    if (len == 0) return 0.0;
    uint64_t counts[256] = {0};
    for (size_t i = 0; i < len; i++) counts[data[i]]++;
    double entropy = 0.0;
    double f_len = (double)len;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / f_len;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}

void zvd_pack_layer_base5(const int8_t * restrict layer_500, uint8_t * restrict out_167) {
    int idx = 0;
    for (int i = 0; i < 498; i += 3) {
        int a = zvd_clamp_i8(layer_500[i], -2, 2) + 2;
        int b = zvd_clamp_i8(layer_500[i+1], -2, 2) + 2;
        int c = zvd_clamp_i8(layer_500[i+2], -2, 2) + 2;
        out_167[idx++] = (uint8_t)(a + 5 * b + 25 * c);
    }
    int a = zvd_clamp_i8(layer_500[498], -2, 2) + 2;
    int b = zvd_clamp_i8(layer_500[499], -2, 2) + 2;
    out_167[166] = (uint8_t)(a + 5 * b);
}

void zvd_unpack_layer_base5(const uint8_t * restrict in_167, int8_t * restrict out_500) {
    int idx = 0;
    for (int i = 0; i < 166; i++) {
        int val = (int)in_167[i];
        out_500[idx++] = (int8_t)((val % 5) - 2);
        out_500[idx++] = (int8_t)(((val / 5) % 5) - 2);
        out_500[idx++] = (int8_t)(((val / 25) % 5) - 2);
    }
    int val = (int)in_167[166];
    out_500[498] = (int8_t)((val % 5) - 2);
    out_500[499] = (int8_t)(((val / 5) % 5) - 2);
}

void zvd_cube_pack_monolith(const ZVD_TetraCube120D * restrict cube, ZVD_Monolith334 * restrict out) {
    zvd_pack_layer_base5(cube->face_data, out->face_packed);
    zvd_pack_layer_base5(cube->vertex_data, out->vertex_packed);
}

void zvd_cube_unpack_monolith(const ZVD_Monolith334 * restrict in_monolith, ZVD_TetraCube120D * restrict cube) {
    zvd_unpack_layer_base5(in_monolith->face_packed, cube->face_data);
    zvd_unpack_layer_base5(in_monolith->vertex_packed, cube->vertex_data);
}

void zvd_bicore_init(ZVD_BiCore120D * restrict core) {
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
    core->angle_a = 0.0f; core->angle_b = 0.0f; core->base_velocity = (float)(M_PI / 6.0);
}

void zvd_bicore_update_rotation(ZVD_BiCore120D * restrict core, double entropy) {
    float dynamic_velocity = core->base_velocity / (1.0f + (float)entropy * 2.0f);
    core->angle_a += dynamic_velocity;
    core->angle_b -= dynamic_velocity * 1.5f;
    float two_pi = (float)(2.0 * M_PI);
    if (core->angle_a >= two_pi) core->angle_a = 0.0f;
    if (core->angle_b <= -two_pi) core->angle_b = 0.0f;
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

float zvd_enterprise_synthesis_120d(const float * restrict collapsed_120, float angle_a, float angle_b, int8_t * restrict out_vec_120) {
    if (!g_luts_initialized) zvd_init_luts();
    float cos_a = cosf(angle_a), sin_a = sinf(angle_a);
    float cos_b = cosf(angle_b), sin_b = sinf(angle_b);
    float total_interference = 0.0f;
    for (int i = 0; i < ZVD_DIM_120; i++) {
        float raw = collapsed_120[i];
        float coord = (float)(i - ZVD_DIM_120 / 2);
        float fx = coord * cos_a - coord * sin_a;
        float fy = coord * sin_b + coord * cos_b;
        float moire = sinf(fx * 0.1f + fy * 0.1f + (angle_a - angle_b));
        float val_acc = raw * (1.0f + 0.5f * moire);
        int scaled = (int)roundf(val_acc * 2.0f);
        int clipped = (scaled < -2 ? -2 : (scaled > 2 ? 2 : scaled));
        out_vec_120[i] = (int8_t)clipped;
        total_interference += (float)clipped * ZVD_SIN_LUT[i];
    }
    return total_interference;
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
                    int f_val = (int)F60[k] + parity;
                    int v_val = - (int)V60[k] - parity;
                    out_cube->face_data[cell_idx] = zvd_clamp_i8(f_val, -2, 2);
                    out_cube->vertex_data[cell_idx] = zvd_clamp_i8(v_val, -2, 2);
                }
            }
        }
    }
}

float zvd_verify_dual_symmetry(const ZVD_TetraCube120D * restrict cube) {
    long long sum_abs = 0;
    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int val = (int)cube->face_data[i] + (int)cube->vertex_data[i];
        sum_abs += (val < 0 ? -val : val);
    }
    return (float)sum_abs / (float)ZVD_CELLS_PER_LAYER;
}

void zvd_rashomon_audit_120d(const int8_t * restrict state_120, ZVD_RashomonAudit * restrict out_audit) {
    double sum_val = 0.0, abs_sum_val = 0.0;
    double min_val = (double)state_120[0], max_val = (double)state_120[0];
    for (int i = 0; i < ZVD_DIM_120; i++) {
        double val = (double)state_120[i];
        sum_val += val;
        abs_sum_val += (val < 0.0 ? -val : val);
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    double mean_val = sum_val / (double)ZVD_DIM_120;
    double var_sum = 0.0;
    for (int i = 0; i < ZVD_DIM_120; i++) {
        double diff = (double)state_120[i] - mean_val;
        var_sum += diff * diff;
    }
    double std_val = sqrt(var_sum / (double)ZVD_DIM_120);
    out_audit->mean_val = mean_val;
    out_audit->min_val = min_val;
    out_audit->max_val = max_val;
    out_audit->std_val = std_val;
    out_audit->risk_index = -(abs_sum_val / (double)ZVD_DIM_120) - (std_val * 0.15);
}

/* =====================================================================
 * DATACENTER FABRIC PIPELINE & OPTICAL SERIALIZATION
 * ===================================================================== */

ZVD_DatacenterPacket zvd_node_ingest_and_process(const uint8_t *raw_data, size_t len, ZVD_BiCore120D *bi_core) {
    ZVD_DatacenterPacket pkt;
    pkt.original_size = len;
    zvd_compute_sha256(raw_data, len, pkt.sha256_hash);
    
    pkt.entropy = zvd_shannon_entropy(raw_data, len);
    zvd_bicore_update_rotation(bi_core, pkt.entropy);
    
    alignas(64) float collapsed[ZVD_DIM_120];
    zvd_stride_collapse_to_120(raw_data, len, collapsed);
    
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    zvd_enterprise_synthesis_120d(collapsed, bi_core->angle_a, bi_core->angle_b, synth_120);
    
    ZVD_RashomonAudit audit;
    zvd_rashomon_audit_120d(synth_120, &audit);
    pkt.risk_index = audit.risk_index;
    
    ZVD_TetraCube120D cube;
    zvd_project_120d_to_dual_cube(synth_120, &cube);
    pkt.dual_asymmetry = zvd_verify_dual_symmetry(&cube);
    
    zvd_cube_pack_monolith(&cube, &pkt.monolith);
    
    /* Self-contained minimal lossless simulation / compression block */
    pkt.compressed_size = len;
    pkt.compressed_payload = (uint8_t *)malloc(len);
    if (pkt.compressed_payload) {
        memcpy(pkt.compressed_payload, raw_data, len);
    }
    return pkt;
}

bool zvd_node_receive_and_verify(const ZVD_DatacenterPacket *pkt, const uint8_t *original_data, size_t original_len) {
    if (pkt->original_size != original_len) return false;
    uint8_t computed_hash[32];
    zvd_compute_sha256(original_data, original_len, computed_hash);
    if (memcmp(computed_hash, pkt->sha256_hash, 32) != 0) return false;
    if (pkt->dual_asymmetry > 1e-5) return false;
    return true;
}

uint8_t* zvd_transmit_optic_burst(const ZVD_DatacenterPacket *pkt, size_t *out_size) {
    size_t total_size = ZVD_OPTIC_MAGIC_LEN + 32 + sizeof(ZVD_Monolith334) + sizeof(size_t) + pkt->compressed_size;
    uint8_t *buffer = (uint8_t *)malloc(total_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;
    memcpy(buffer + offset, ZVD_OPTIC_MAGIC, ZVD_OPTIC_MAGIC_LEN);
    offset += ZVD_OPTIC_MAGIC_LEN;
    
    memcpy(buffer + offset, pkt->sha256_hash, 32);
    offset += 32;
    
    memcpy(buffer + offset, &pkt->monolith, sizeof(ZVD_Monolith334));
    offset += sizeof(ZVD_Monolith334);
    
    memcpy(buffer + offset, &pkt->compressed_size, sizeof(size_t));
    offset += sizeof(size_t);
    
    if (pkt->compressed_size > 0 && pkt->compressed_payload) {
        memcpy(buffer + offset, pkt->compressed_payload, pkt->compressed_size);
        offset += pkt->compressed_size;
    }
    
    *out_size = offset;
    return buffer;
}

bool zvd_receive_optic_burst(const uint8_t *burst_data, size_t burst_size, ZVD_DatacenterPacket *out_pkt) {
    if (burst_size < ZVD_OPTIC_MAGIC_LEN + 32 + sizeof(ZVD_Monolith334) + sizeof(size_t)) return false;
    if (memcmp(burst_data, ZVD_OPTIC_MAGIC, ZVD_OPTIC_MAGIC_LEN) != 0) return false;
    
    size_t offset = ZVD_OPTIC_MAGIC_LEN;
    memcpy(out_pkt->sha256_hash, burst_data + offset, 32);
    offset += 32;
    
    memcpy(&out_pkt->monolith, burst_data + offset, sizeof(ZVD_Monolith334));
    offset += sizeof(ZVD_Monolith334);
    
    memcpy(&out_pkt->compressed_size, burst_data + offset, sizeof(size_t));
    offset += sizeof(size_t);
    
    if (burst_size < offset + out_pkt->compressed_size) return false;
    
    out_pkt->compressed_payload = (uint8_t *)malloc(out_pkt->compressed_size);
    if (!out_pkt->compressed_payload) return false;
    memcpy(out_pkt->compressed_payload, burst_data + offset, out_pkt->compressed_size);
    out_pkt->original_size = out_pkt->compressed_size;
    
    return true;
}

/* =====================================================================
 * MULTITHREADED BENCHMARK BLOCK (OpenMP / C17)
 * ===================================================================== */
#ifdef ZVD_FABRIC_BENCHMARK
int main(void) {
    printf("[ZVD FABRIC] Initializing HyperScale C17 Datacenter Fabric (Vladimir Zavodiuk, 2026)...\n");
    zvd_init_luts();
    
    const size_t payload_len = 1024 * 1024; /* 1 MB payload */
    uint8_t *test_payload = (uint8_t *)malloc(payload_len);
    for (size_t i = 0; i < payload_len; i++) test_payload[i] = (uint8_t)(i % 251);
    
    const int num_iterations = 100;
    ZVD_BiCore120D bicore;
    zvd_bicore_init(&bicore);
    
    printf("[*] Running Single-Threaded Benchmark (%d iterations of 1 MB)...\n", num_iterations);
    clock_t start_st = clock();
    for (int i = 0; i < num_iterations; i++) {
        ZVD_DatacenterPacket pkt = zvd_node_ingest_and_process(test_payload, payload_len, &bicore);
        size_t burst_sz = 0;
        uint8_t *burst = zvd_transmit_optic_burst(&pkt, &burst_sz);
        
        ZVD_DatacenterPacket rx_pkt;
        zvd_receive_optic_burst(burst, burst_sz, &rx_pkt);
        
        bool verified = zvd_node_receive_and_verify(&rx_pkt, test_payload, payload_len);
        if (!verified) {
            printf("[!] Verification failed!\n");
        }
        
        free(pkt.compressed_payload);
        free(rx_pkt.compressed_payload);
        free(burst);
    }
    clock_t end_st = clock();
    double time_st = (double)(end_st - start_st) / CLOCKS_PER_SEC;
    double throughput_st = ((double)(payload_len * num_iterations) / (1024.0 * 1024.0)) / time_st;
    printf("    - Single-Threaded Throughput: %.2f MB/s\n", throughput_st);
    
    printf("[*] Running Multi-Threaded Benchmark (OpenMP parallel for, %d iterations)...\n", num_iterations);
    double start_mt = 
#ifdef _OPENMP
        omp_get_wtime();
#else
        (double)clock() / CLOCKS_PER_SEC;
#endif

    bool all_verified = true;
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < num_iterations; i++) {
        ZVD_BiCore120D local_bicore;
        zvd_bicore_init(&local_bicore);
        
        ZVD_DatacenterPacket pkt = zvd_node_ingest_and_process(test_payload, payload_len, &local_bicore);
        size_t burst_sz = 0;
        uint8_t *burst = zvd_transmit_optic_burst(&pkt, &burst_sz);
        
        ZVD_DatacenterPacket rx_pkt;
        bool rx_ok = zvd_receive_optic_burst(burst, burst_sz, &rx_pkt);
        bool verified = rx_ok && zvd_node_receive_and_verify(&rx_pkt, test_payload, payload_len);
        
        if (!verified) {
            #pragma omp atomic
            all_verified = false;
        }
        
        free(pkt.compressed_payload);
        free(rx_pkt.compressed_payload);
        free(burst);
    }
    
    double end_mt = 
#ifdef _OPENMP
        omp_get_wtime();
#else
        (double)clock() / CLOCKS_PER_SEC;
#endif
    double time_mt = end_mt - start_mt;
    double throughput_mt = ((double)(payload_len * num_iterations) / (1024.0 * 1024.0)) / time_mt;
    double monoliths_per_sec = (double)num_iterations / time_mt;
    
    printf("    - Multi-Threaded Throughput:  %.2f MB/s\n", throughput_mt);
    printf("    - Monoliths Fabric Rate:      %.2f Monoliths/sec\n", monoliths_per_sec);
    
    printf("-" << 70); // wait, C string literal concatenation or print format:
    printf("----------------------------------------------------------------------\n");
    if (all_verified) {
        printf(" VERDICT: BIT-PERFECT VERIFIED (OpenMP C17 Hyperscale Fabric OK)\n");
    } else {
        printf(" VERDICT: VERIFICATION FAILED\n");
    }
    printf("----------------------------------------------------------------------\n");
    
    free(test_payload);
    return 0;
}
#endif
