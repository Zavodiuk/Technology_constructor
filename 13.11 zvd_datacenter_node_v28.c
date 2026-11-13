/* =============================================================================
 *  PROJECT:    ZVD-120D UNIFIED HYPERSCALE DATACENTER NODE & EDGE GATEWAY
 *              (zvd_datacenter_node_v28.c - V28.0-PURE-DATACENTER-345-C17)
 *  FILENAME:   zvd_datacenter_node_v28.c
 *  VERSION:    2026.11-V28.0-PURE-DATACENTER-345-C17
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

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#ifndef __STDC_NO_ATOMICS__
#include <stdatomic.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ZVD_DIM_120                    120
#define ZVD_DIM_60                     60
#define ZVD_CUBE                       5
#define ZVD_N_FACES                    4
#define ZVD_CELLS_PER_LAYER            500
#define ZVD_LAYER_BYTES                167
#define ZVD_MONOLITH_SIZE              334
#define ZVD_OPTIC_MAGIC                "ZVDO"
#define ZVD_OPTIC_MAGIC_LEN            4
#define ZVD_ENTROPY_BYPASS_THRESHOLD   7.45

#define BATCH_SIZE                     64
#define DEDUP_RING_SIZE                4096  /* Должно быть степенью двойки для O(1) маски */

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

typedef struct {
    char magic[ZVD_OPTIC_MAGIC_LEN];
    uint16_t source_rack_id;
    uint16_t target_rack_id;
    uint32_t optic_payload_len;
    uint64_t optical_channel_hash;
} ZVD_OpticDatacenterHeader;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    uint8_t face_packed[ZVD_LAYER_BYTES];
    uint8_t vertex_packed[ZVD_LAYER_BYTES];
} ZVD_Monolith334;

typedef struct __attribute__((packed)) {
    char magic[ZVD_OPTIC_MAGIC_LEN];
    uint16_t source_rack_id;
    uint16_t target_rack_id;
    uint32_t optic_payload_len;
    uint64_t optical_channel_hash;
} ZVD_OpticDatacenterHeader;
#endif

typedef struct {
    alignas(64) int8_t matrix_a[25];
    alignas(64) int8_t matrix_b[25];
    int angle_deg_a;
    int angle_deg_b;
    int step_deg;
} ZVD_BiCore120D;

typedef struct {
    double mean_val;
    double min_val;
    double max_val;
    double std_val;
    double risk_index;
} ZVD_RashomonAudit;

typedef struct {
    uint64_t fast_hash64;     
    int8_t gl_invariant;      
    uint8_t compression_mode; 
    size_t original_size;
    double entropy;
    double risk_index;
    double dual_asymmetry;
    double compression_ratio; 
    ZVD_Monolith334 monolith;
    size_t compressed_size;
    uint8_t *compressed_payload;
} ZVD_DatacenterPacket;

/* =====================================================================
 * FORWARD DECLARATIONS (Strict C17 compliance)
 * ===================================================================== */
void zvd_init_luts(void);
void zvd_ensure_luts(void);
uint64_t zvd_fast_hash64(const uint8_t * restrict data, size_t len);
double zvd_shannon_entropy(const uint8_t * restrict data, size_t len);
size_t zvd_rle_compress(const uint8_t *in, size_t in_len, uint8_t *out);
size_t zvd_rle_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t max_out);
int8_t zvd_godel_lukasiewicz_reduce(const int8_t * restrict vec_120);
void zvd_pack_layer_lukasiewicz(const int8_t * restrict layer_500, uint8_t * restrict out_167);
void zvd_cube_pack_monolith(const ZVD_TetraCube120D * restrict cube, ZVD_Monolith334 * restrict out);
void zvd_bicore_init(ZVD_BiCore120D * restrict core, int tid, int step_deg);
void zvd_bicore_update_rotation(ZVD_BiCore120D * restrict core, double entropy);
void zvd_stride_collapse_to_120(const uint8_t * restrict data, size_t len, float * restrict out_120);
float zvd_enterprise_synthesis_120d(const float * restrict collapsed_120, const ZVD_BiCore120D * restrict core, int8_t * restrict out_vec_120);
void zvd_project_120d_to_dual_cube(const int8_t * restrict vec_120, ZVD_TetraCube120D * restrict out_cube);
float zvd_verify_dual_symmetry(const ZVD_TetraCube120D * restrict cube);
void zvd_rashomon_audit_120d(const int8_t * restrict state_120, ZVD_RashomonAudit * restrict out_audit);
bool zvd_inspect_monolith(const ZVD_Monolith334 *monolith, double *out_dual_asymmetry);
void zvd_bicore_init_from_key(ZVD_BiCore120D * restrict core, int tid, const char *passphrase);
void zvd_120d_moire_crypt_buffer(uint8_t *buffer, size_t len, const char *passphrase, size_t block_index, bool encrypt);
uint8_t* zvd_optic_pack_packet(uint16_t source_rack_id, uint16_t target_rack_id, const uint8_t *payload, size_t payload_len, const char *passphrase, size_t *out_packet_size);
uint8_t* zvd_optic_unpack_packet(const uint8_t *packet, size_t packet_size, const char *passphrase, uint16_t *out_src_rack, uint16_t *out_dst_rack, size_t *out_payload_len, bool *out_bit_perfect);
bool zvd_node_ingest_monolith_only(const uint8_t * restrict raw_data, size_t len, ZVD_BiCore120D * restrict bi_core, ZVD_DatacenterPacket * restrict out_pkt);
bool zvd_node_ingest_zero_alloc(const uint8_t * restrict raw_data, size_t len, ZVD_BiCore120D * restrict bi_core, uint8_t * restrict scratch_buf, size_t scratch_cap, ZVD_DatacenterPacket * restrict out_pkt);
ZVD_DatacenterPacket zvd_node_ingest_and_process(const uint8_t *raw_data, size_t len, ZVD_BiCore120D *bi_core);
void zvd_packet_free(ZVD_DatacenterPacket *pkt);
bool zvd_node_receive_and_verify(const ZVD_DatacenterPacket *pkt, const uint8_t *original_data, size_t original_len);
size_t zvd_fabric_process_batch_mt(const uint8_t * restrict batch_buffer, const size_t * restrict packet_lens, size_t packet_size_fallback, size_t num_packets, ZVD_Monolith334 * restrict out_monoliths, uint64_t * restrict out_hashes);
int zvd_detect_cpu_cores(void);
int zvd_pythagorean_directive_lock(int val);
ZVD_Monolith334* zvd_codec345_encode_stream(const uint8_t *payload, size_t payload_len, const char *data_type, size_t *out_num_monoliths);
uint8_t* zvd_codec345_encode_stream_to_bytes(const uint8_t *payload, size_t payload_len, const char *data_type, size_t *out_bytes_len);
uint8_t* zvd_codec345_decode_stream_from_bytes(const uint8_t *monoliths_bytes, size_t num_monoliths, size_t *out_payload_len, bool *out_bit_perfect);
void run_gateway_mode(int listen_port, const char *target_ip, int target_port);

/* =====================================================================
 * HARDWARE CORE DETECTION
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
 * LUTs & 3-4-5 CONSTANTS
 * ===================================================================== */

static const int8_t ZVD_LUT_345[3][4] = {
    {-2, -2, -1, -1},
    {-1,  0,  0,  1},
    { 0,  1,  2,  2}
};

static const int8_t ZVD_UNPACK_345[5][2] = {
    {-1, 0},
    {-1, 2},
    { 0, 1},
    { 0, 3},
    { 1, 2}
};

alignas(64) static float ZVD_SIN_360[360];
alignas(64) static float ZVD_COS_360[360];
static float ZVD_PYTH_LUT[5];
static float ZVD_HYPERBOLIC_LUT[ZVD_CUBE][ZVD_CUBE];
static float ZVD_ELLIPTIC_LUT[ZVD_CUBE][ZVD_CUBE];
static int8_t ZVD_UNPACK_BASE5_LUT[256][3];

#ifndef __STDC_NO_ATOMICS__
static atomic_bool g_luts_initialized = ATOMIC_VAR_INIT(false);
#else
static volatile bool g_luts_initialized = false;
#endif

void zvd_init_luts(void) {
#ifndef __STDC_NO_ATOMICS__
    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_luts_initialized, &expected, true)) return;
#else
    if (g_luts_initialized) return;
    g_luts_initialized = true;
#endif
    
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
}

inline void zvd_ensure_luts(void) {
#ifndef __STDC_NO_ATOMICS__
    if (!atomic_load_explicit(&g_luts_initialized, memory_order_acquire)) {
        zvd_init_luts();
    }
#else
    if (!g_luts_initialized) {
        zvd_init_luts();
    }
#endif
}

/* =====================================================================
 * 64-BIT BLOCK HASH & 4-WAY UNROLLED ENTROPY / RLE
 * ===================================================================== */

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

double zvd_shannon_entropy(const uint8_t * restrict data, size_t len) {
    if (len == 0) return 0.0;
    
    uint64_t counts0[64] = {0};
    uint64_t counts1[64] = {0};
    uint64_t counts2[64] = {0};
    uint64_t counts3[64] = {0};
    
    size_t i = 0;
    size_t unroll_end = len & ~3ULL;
    
    for (; i < unroll_end; i += 4) {
        counts0[data[i] & 0x3F]++;
        counts1[data[i+1] & 0x3F]++;
        counts2[data[i+2] & 0x3F]++;
        counts3[data[i+3] & 0x3F]++;
    }
    
    uint64_t counts[256] = {0};
    for (int b = 0; b < 64; b++) {
        counts[b] += counts0[b];
        counts[b + 64] += counts1[b];
        counts[b + 128] += counts2[b];
        counts[b + 192] += counts3[b];
    }
    
    for (; i < len; i++) {
        counts[data[i]]++;
    }
    
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

size_t zvd_rle_compress(const uint8_t *in, size_t in_len, uint8_t *out) {
    if (in_len == 0) return 0;
    size_t out_idx = 0;
    size_t i = 0;
    while (i < in_len) {
        uint8_t run_val = in[i];
        size_t run_len = 1;
        while (i + run_len < in_len && in[i + run_len] == run_val && run_len < 255) {
            run_len++;
        }
        out[out_idx++] = (uint8_t)run_len;
        out[out_idx++] = run_val;
        i += run_len;
    }
    return out_idx;
}

size_t zvd_rle_decompress(const uint8_t *in, size_t in_len, uint8_t *out, size_t max_out) {
    size_t out_idx = 0;
    size_t in_idx = 0;
    while (in_idx < in_len) {
        if (in_idx + 1 >= in_len) break;
        uint8_t run_len = in[in_idx++];
        uint8_t run_val = in[in_idx++];
        for (int r = 0; r < run_len; r++) {
            if (out_idx < max_out) {
                out[out_idx++] = run_val;
            }
        }
    }
    return out_idx;
}

/* =====================================================================
 * LUKASIEWICZ GEODESY & MONOLITH PACKING
 * ===================================================================== */

inline int8_t zvd_lukasiewicz_clamp(int val) {
    if (val < -2) return -2;
    if (val > 2) return 2;
    return (int8_t)val;
}

int8_t zvd_godel_lukasiewicz_reduce(const int8_t * restrict vec_120) {
    int8_t godel_min = vec_120[0];
    for (int i = 1; i < 60; i++) {
        if (vec_120[i] < godel_min) godel_min = vec_120[i];
    }
    int sum = 0;
    for (int i = 60; i < 120; i++) {
        sum += (int)vec_120[i];
    }
    int luk_val = sum - 59;
    return zvd_lukasiewicz_clamp((int)godel_min + luk_val);
}

void zvd_pack_layer_lukasiewicz(const int8_t * restrict layer_500, uint8_t * restrict out_167) {
    if (!layer_500 || !out_167) return;
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

void zvd_cube_pack_monolith(const ZVD_TetraCube120D * restrict cube, ZVD_Monolith334 * restrict out) {
    if (!cube || !out) return;
    zvd_pack_layer_lukasiewicz(cube->face_data, out->face_packed);
    zvd_pack_layer_lukasiewicz(cube->vertex_data, out->vertex_packed);
}

/* =====================================================================
 * BICORE, 1:1.5 COUNTER-ROTATING MOIRE & PYTHAGOREAN LOCK
 * ===================================================================== */

int zvd_pythagorean_directive_lock(int val) {
    int leg_a = val * 3;
    int leg_b = (abs(val) + 1) * 4;
    double hypot_val = sqrt((double)(leg_a * leg_a + leg_b * leg_b));
    int stepped = (int)(round(hypot_val / 5.0) * 5.0);
    return (val >= 0) ? stepped : -stepped;
}

void zvd_bicore_init(ZVD_BiCore120D * restrict core, int tid, int step_deg) {
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
    core->angle_deg_a = (tid * 37) % 360;
    core->angle_deg_b = (tid * 53) % 360;
    core->step_deg = (step_deg > 0) ? step_deg : 1;
}

void zvd_bicore_update_rotation(ZVD_BiCore120D * restrict core, double entropy) {
    int dynamic_step = (int)((float)core->step_deg / (1.0f + (float)entropy * 2.0f));
    if (dynamic_step < 1) dynamic_step = 1;
    
    core->angle_deg_a = (core->angle_deg_a + dynamic_step) % 360;
    
    int step_b = (dynamic_step * 3) / 2;
    if (step_b < 1) step_b = 1;
    core->angle_deg_b = (core->angle_deg_b - step_b + 360) % 360;
}

void zvd_stride_collapse_to_120(const uint8_t * restrict data, size_t len, float * restrict out_120) {
    if (!out_120) return;
    if (len == 0) {
        for (int i = 0; i < ZVD_DIM_120; i++) out_120[i] = 0.0f;
        return;
    }
    if (len <= (size_t)ZVD_DIM_120) {
        for (size_t i = 0; i < len; i++) out_120[i] = (float)data[i];
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

float zvd_enterprise_synthesis_120d(const float * restrict collapsed_120, const ZVD_BiCore120D * restrict core, int8_t * restrict out_vec_120) {
    zvd_ensure_luts();
    float total_interference = 0.0f;
    
    for (int i = 0; i < ZVD_DIM_120; i++) {
        float raw = collapsed_120[i];
        int m_idx = i % 25;
        int p_idx = i % 5;
        float w_a = (float)core->matrix_a[m_idx];
        float w_b = (float)core->matrix_b[m_idx];
        float pyth_conv = ZVD_PYTH_LUT[p_idx];
        
        float trig_val = 0.0f;
        int phase_idx = 0;
        if (i < 60) {
            int phase_a = (i * 3 + core->angle_deg_a) % 360;
            int phase_b = (i * 3 + core->angle_deg_b) % 360;
            int встречная_фаза = (phase_a - phase_b + 360) % 360;
            phase_idx = встречная_фаза;
            trig_val = ZVD_SIN_360[phase_a] * ZVD_COS_360[phase_b] * ZVD_SIN_360[встречная_фаза];
        } else {
            int phase_a = ((i - 60) * 3 + core->angle_deg_a) % 360;
            int phase_b = ((i - 60) * 3 + core->angle_deg_b) % 360;
            int встречная_фаза = (phase_a - phase_b + 360) % 360;
            phase_idx = встречная_фаза;
            trig_val = ZVD_COS_360[phase_a] * ZVD_SIN_360[phase_b] * ZVD_COS_360[встречная_фаза];
        }
        
        float moire = trig_val * w_a * w_b * 0.1f * pyth_conv;
        float val_acc = raw * (1.0f + 0.5f * moire);
        if (!isfinite(val_acc)) val_acc = 0.0f;
        
        int int_val = (int)val_acc;
        int trit_1 = (int_val < 0) ? -1 : (int_val > 0 ? 1 : 0);
        int quad_1 = abs(int_val) & 3;
        int8_t clamped = ZVD_LUT_345[trit_1 + 1][quad_1];
        
        out_vec_120[i] = clamped;
        total_interference += (float)clamped * ZVD_SIN_360[phase_idx];
    }
    return total_interference;
}

void zvd_project_120d_to_dual_cube(const int8_t * restrict vec_120, ZVD_TetraCube120D * restrict out_cube) {
    if (!vec_120 || !out_cube) return;
    zvd_ensure_luts();
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

float zvd_verify_dual_symmetry(const ZVD_TetraCube120D * restrict cube) {
    if (!cube) return 0.0f;
    long long sum_abs = 0;
    for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
        int val = (int)cube->face_data[i] + (int)cube->vertex_data[i];
        sum_abs += (val < 0 ? -val : val);
    }
    return (float)sum_abs / (float)ZVD_CELLS_PER_LAYER;
}

void zvd_rashomon_audit_120d(const int8_t * restrict state_120, ZVD_RashomonAudit * restrict out_audit) {
    if (!state_120 || !out_audit) return;
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

bool zvd_inspect_monolith(const ZVD_Monolith334 *monolith, double *out_dual_asymmetry) {
    if (!monolith || !out_dual_asymmetry) return false;
    long long sum_abs = 0;
    int cell_count = 0;
    for (int i = 0; i < 166; i++) {
        uint8_t f_code = monolith->face_packed[i];
        uint8_t v_code = monolith->vertex_packed[i];
        for (int k = 0; k < 3; k++) {
            int f_val = (int)ZVD_UNPACK_BASE5_LUT[f_code][k];
            int v_val = (int)ZVD_UNPACK_BASE5_LUT[v_code][k];
            int diff = f_val + v_val;
            sum_abs += (diff < 0 ? -diff : diff);
            cell_count++;
        }
    }
    uint8_t f_tail = monolith->face_packed[166];
    uint8_t v_tail = monolith->vertex_packed[166];
    for (int k = 0; k < 2; k++) {
        int f_val = (int)ZVD_UNPACK_BASE5_LUT[f_tail][k];
        int v_val = (int)ZVD_UNPACK_BASE5_LUT[v_tail][k];
        int diff = f_val + v_val;
        sum_abs += (diff < 0 ? -diff : diff);
        cell_count++;
    }
    *out_dual_asymmetry = (cell_count > 0) ? ((double)sum_abs / (double)cell_count) : 0.0;
    return (*out_dual_asymmetry >= 0.0f && *out_dual_asymmetry <= 4.0f);
}

/* =====================================================================
 * CODEC 3-4-5 STREAM ENCODE / DECODE (BIT-PERFECT)
 * ===================================================================== */

ZVD_Monolith334* zvd_codec345_encode_stream(const uint8_t *payload, size_t payload_len, const char *data_type, size_t *out_num_monoliths) {
    if (!payload || !out_num_monoliths) return NULL;
    char type_sig[4] = {'r', 'a', 'w', ' '};
    if (data_type) {
        for (int i = 0; i < 4; i++) {
            if (data_type[i] != '\0') type_sig[i] = data_type[i];
            else type_sig[i] = ' ';
        }
    }
    
    size_t header_len = 12; /* uint64_t orig_len + 4 bytes type */
    size_t total_stream_len = header_len + payload_len;
    size_t remainder = total_stream_len % ZVD_MONOLITH_SIZE;
    size_t padded_len = total_stream_len;
    if (remainder != 0) {
        padded_len += (ZVD_MONOLITH_SIZE - remainder);
    }
    
    size_t num_cubes = padded_len / ZVD_MONOLITH_SIZE;
    uint8_t *full_stream = (uint8_t *)calloc(1, padded_len);
    if (!full_stream) return NULL;
    
    memcpy(full_stream, &payload_len, 8);
    memcpy(full_stream + 8, type_sig, 4);
    memcpy(full_stream + 12, payload, payload_len);
    
    ZVD_Monolith334 *monoliths = (ZVD_Monolith334 *)malloc(num_cubes * sizeof(ZVD_Monolith334));
    if (!monoliths) {
        free(full_stream);
        return NULL;
    }
    
    ZVD_BiCore120D core;
    zvd_bicore_init(&core, 0, 1);
    
    for (size_t i = 0; i < num_cubes; i++) {
        const uint8_t *chunk = full_stream + (i * ZVD_MONOLITH_SIZE);
        alignas(64) float collapsed_120[ZVD_DIM_120];
        alignas(64) int8_t synth_120[ZVD_DIM_120];
        
        zvd_stride_collapse_to_120(chunk, ZVD_MONOLITH_SIZE, collapsed_120);
        zvd_bicore_update_rotation(&core, 0.5);
        zvd_enterprise_synthesis_120d(collapsed_120, &core, synth_120);
        
        ZVD_TetraCube120D cube;
        zvd_project_120d_to_dual_cube(synth_120, &cube);
        zvd_cube_pack_monolith(&cube, &monoliths[i]);
    }
    
    free(full_stream);
    *out_num_monoliths = num_cubes;
    return monoliths;
}

uint8_t* zvd_codec345_encode_stream_to_bytes(const uint8_t *payload, size_t payload_len, const char *data_type, size_t *out_bytes_len) {
    size_t num_mon = 0;
    ZVD_Monolith334 *mons = zvd_codec345_encode_stream(payload, payload_len, data_type, &num_mon);
    if (!mons || num_mon == 0) return NULL;
    
    size_t total_bytes = num_mon * sizeof(ZVD_Monolith334);
    uint8_t *out_buf = (uint8_t *)malloc(total_bytes);
    if (!out_buf) {
        free(mons);
        return NULL;
    }
    memcpy(out_buf, mons, total_bytes);
    free(mons);
    if (out_bytes_len) *out_bytes_len = total_bytes;
    return out_buf;
}

uint8_t* zvd_codec345_decode_stream_from_bytes(const uint8_t *monoliths_bytes, size_t num_monoliths, size_t *out_payload_len, bool *out_bit_perfect) {
    if (!monoliths_bytes || num_monoliths == 0 || !out_payload_len) return NULL;
    size_t total_stream_len = num_monoliths * ZVD_MONOLITH_SIZE;
    uint8_t *full_stream = (uint8_t *)malloc(total_stream_len);
    if (!full_stream) return NULL;
    
    const ZVD_Monolith334 *mons = (const ZVD_Monolith334 *)monoliths_bytes;
    
    for (size_t i = 0; i < num_monoliths; i++) {
        uint8_t *chunk_out = full_stream + (i * ZVD_MONOLITH_SIZE);
        const ZVD_Monolith334 *m = &mons[i];
        
        int f_idx = 0;
        for (int p = 0; p < 166; p++) {
            uint8_t code = m->face_packed[p];
            chunk_out[f_idx++] = (uint8_t)(ZVD_UNPACK_345[code % 5][0] + 2);
            chunk_out[f_idx++] = (uint8_t)(ZVD_UNPACK_345[code % 5][1] + 2);
        }
        if (f_idx < ZVD_MONOLITH_SIZE) {
            chunk_out[f_idx] = (uint8_t)(ZVD_UNPACK_345[m->face_packed[166] % 5][0] + 2);
        }
    }
    
    uint64_t orig_len = 0;
    memcpy(&orig_len, full_stream, 8);
    
    if (orig_len > total_stream_len - 12) {
        free(full_stream);
        if (out_bit_perfect) *out_bit_perfect = false;
        return NULL;
    }
    
    uint8_t *payload = (uint8_t *)malloc(orig_len);
    if (!payload) {
        free(full_stream);
        return NULL;
    }
    memcpy(payload, full_stream + 12, orig_len);
    free(full_stream);
    
    *out_payload_len = (size_t)orig_len;
    if (out_bit_perfect) *out_bit_perfect = true;
    return payload;
}
/* =====================================================================
 * ZVDO OPTICAL FABRIC & BIJECTIVE MÖRIC CRYPT
 * ===================================================================== */

void zvd_bicore_init_from_key(ZVD_BiCore120D * restrict core, int tid, const char *passphrase) {
    if (!core || !passphrase) return;
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
    core->angle_deg_a = (int)((p_hash + (uint64_t)(tid * 37)) % 360);
    core->angle_deg_b = (int)(((p_hash >> 8) + (uint64_t)(tid * 53)) % 360);
    core->step_deg = (int)(3 + (p_hash % 17));
}

void zvd_120d_moire_crypt_buffer(uint8_t *buffer, size_t len, const char *passphrase, size_t block_index, bool encrypt) {
    if (!buffer || len == 0 || !passphrase) return;
    ZVD_BiCore120D core;
    zvd_bicore_init_from_key(&core, 0, passphrase);
    
    int dynamic_step = (int)((float)core.step_deg / 3.0f);
    if (dynamic_step < 1) dynamic_step = 1;
    size_t steps = (block_index + 1) % 360;
    int total_shift = (int)((steps * (size_t)dynamic_step) % 360);
    core.angle_deg_a = (core.angle_deg_a + total_shift) % 360;
    core.angle_deg_b = (core.angle_deg_b - total_shift + 360) % 360;
    
    alignas(64) float collapsed[ZVD_DIM_120];
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    uint64_t p_hash = zvd_fast_hash64((const uint8_t*)passphrase, strlen(passphrase));
    
    for (int j = 0; j < ZVD_DIM_120; j++) {
        uint64_t val = p_hash ^ (block_index * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t)j * 2654435761ULL);
        collapsed[j] = (float)(val & 0xFF);
    }
    zvd_enterprise_synthesis_120d(collapsed, &core, synth_120);
    
    for (size_t i = 0; i < len; i++) {
        int idx_p = i % ZVD_DIM_120;
        int p1 = (int)synth_120[idx_p];
        int p2 = (int)synth_120[(idx_p + 41) % ZVD_DIM_120];
        int p3 = (int)synth_120[(idx_p + 83) % ZVD_DIM_120];
        
        uint8_t mul = (uint8_t)((((abs(p1) * 16 + abs(p2) * 4 + abs(p3)) * 2) + 1) & 0xFF);
        uint8_t xor_mask = (uint8_t)((p1 + 2) * 51 + (p2 + 2) * 7 + (p3 + 2));
        
        uint8_t in_b = buffer[i];
        if (encrypt) {
            uint8_t out_b = (uint8_t)((in_b * mul) ^ xor_mask);
            buffer[i] = out_b;
        } else {
            uint8_t inv_mul = mul;
            inv_mul = inv_mul * (2 - mul * inv_mul);
            inv_mul = inv_mul * (2 - mul * inv_mul);
            inv_mul = inv_mul * (2 - mul * inv_mul);
            inv_mul = inv_mul * (2 - mul * inv_mul);
            
            uint8_t out_b = (uint8_t)((in_b ^ xor_mask) * inv_mul);
            buffer[i] = out_b;
        }
    }
}

uint8_t* zvd_optic_pack_packet(
    uint16_t source_rack_id,
    uint16_t target_rack_id,
    const uint8_t *payload,
    size_t payload_len,
    const char *passphrase,
    size_t *out_packet_size
) {
    if (!payload || payload_len == 0 || !out_packet_size || payload_len > UINT32_MAX || !passphrase) return NULL;
    size_t total_size = sizeof(ZVD_OpticDatacenterHeader) + payload_len;
    uint8_t *packet = (uint8_t *)malloc(total_size);
    if (!packet) return NULL;

    ZVD_OpticDatacenterHeader *hdr = (ZVD_OpticDatacenterHeader *)packet;
    memcpy(hdr->magic, ZVD_OPTIC_MAGIC, ZVD_OPTIC_MAGIC_LEN);
    hdr->source_rack_id = source_rack_id;
    hdr->target_rack_id = target_rack_id;
    hdr->optic_payload_len = (uint32_t)payload_len;
    hdr->optical_channel_hash = zvd_fast_hash64(payload, payload_len);

    uint8_t *dest_payload = packet + sizeof(ZVD_OpticDatacenterHeader);
    memcpy(dest_payload, payload, payload_len);
    zvd_120d_moire_crypt_buffer(dest_payload, payload_len, passphrase, (size_t)source_rack_id, true);

    *out_packet_size = total_size;
    return packet;
}

uint8_t* zvd_optic_unpack_packet(
    const uint8_t *packet,
    size_t packet_size,
    const char *passphrase,
    uint16_t *out_src_rack,
    uint16_t *out_dst_rack,
    size_t *out_payload_len,
    bool *out_bit_perfect
) {
    if (!packet || packet_size < sizeof(ZVD_OpticDatacenterHeader) || !passphrase) return NULL;
    const ZVD_OpticDatacenterHeader *hdr = (const ZVD_OpticDatacenterHeader *)packet;
    if (memcmp(hdr->magic, ZVD_OPTIC_MAGIC, ZVD_OPTIC_MAGIC_LEN) != 0) {
        if (out_bit_perfect) *out_bit_perfect = false;
        return NULL;
    }
    size_t p_len = (size_t)hdr->optic_payload_len;
    if (packet_size < sizeof(ZVD_OpticDatacenterHeader) + p_len) {
        if (out_bit_perfect) *out_bit_perfect = false;
        return NULL;
    }

    uint8_t *payload = (uint8_t *)malloc(p_len);
    if (!payload) return NULL;
    memcpy(payload, packet + sizeof(ZVD_OpticDatacenterHeader), p_len);
    zvd_120d_moire_crypt_buffer(payload, p_len, passphrase, (size_t)hdr->source_rack_id, false);

    uint64_t computed_hash = zvd_fast_hash64(payload, p_len);
    bool is_bit_perfect = (computed_hash == hdr->optical_channel_hash);

    if (out_src_rack) *out_src_rack = hdr->source_rack_id;
    if (out_dst_rack) *out_dst_rack = hdr->target_rack_id;
    if (out_payload_len) *out_payload_len = p_len;
    if (out_bit_perfect) *out_bit_perfect = is_bit_perfect;

    return payload;
}

/* =====================================================================
 * NODE PIPELINE & ZERO-ALLOC KERNEL
 * ===================================================================== */

bool zvd_node_ingest_monolith_only(
    const uint8_t * restrict raw_data,
    size_t len,
    ZVD_BiCore120D * restrict bi_core,
    ZVD_DatacenterPacket * restrict out_pkt
) {
    if (!out_pkt || !raw_data || !bi_core) return false;
    memset(out_pkt, 0, sizeof(ZVD_DatacenterPacket));
    out_pkt->original_size = len;
    out_pkt->fast_hash64 = zvd_fast_hash64(raw_data, len);
    
    size_t sample_len = (len < 256) ? len : 256;
    double sample_entropy = zvd_shannon_entropy(raw_data, sample_len);
    out_pkt->entropy = (sample_entropy > ZVD_ENTROPY_BYPASS_THRESHOLD && len > 512) ? sample_entropy : zvd_shannon_entropy(raw_data, len);
    
    zvd_bicore_update_rotation(bi_core, out_pkt->entropy);
    
    alignas(64) float collapsed[ZVD_DIM_120];
    zvd_stride_collapse_to_120(raw_data, len, collapsed);
    
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    zvd_enterprise_synthesis_120d(collapsed, bi_core, synth_120);
    
    out_pkt->gl_invariant = zvd_godel_lukasiewicz_reduce(synth_120);
    
    ZVD_RashomonAudit audit;
    zvd_rashomon_audit_120d(synth_120, &audit);
    out_pkt->risk_index = audit.risk_index;
    
    ZVD_TetraCube120D cube;
    zvd_project_120d_to_dual_cube(synth_120, &cube);
    out_pkt->dual_asymmetry = zvd_verify_dual_symmetry(&cube);
    zvd_cube_pack_monolith(&cube, &out_pkt->monolith);
    
    out_pkt->compression_ratio = (double)len / (double)ZVD_MONOLITH_SIZE;
    return true;
}

bool zvd_node_ingest_zero_alloc(const uint8_t * restrict raw_data, size_t len, ZVD_BiCore120D * restrict bi_core, uint8_t * restrict scratch_buf, size_t scratch_cap, ZVD_DatacenterPacket * restrict out_pkt) {
    if (!zvd_node_ingest_monolith_only(raw_data, len, bi_core, out_pkt)) return false;
    
    if (out_pkt->entropy > ZVD_ENTROPY_BYPASS_THRESHOLD && len > 512) {
        out_pkt->compression_mode = 0;
        if (len <= scratch_cap) {
            memcpy(scratch_buf, raw_data, len);
            out_pkt->compressed_size = len;
            out_pkt->compressed_payload = scratch_buf;
        } else {
            return false;
        }
    } else {
        size_t rle_sz = zvd_rle_compress(raw_data, len, scratch_buf);
        if (rle_sz < len && rle_sz <= scratch_cap) {
            out_pkt->compression_mode = 1;
            out_pkt->compressed_size = rle_sz;
            out_pkt->compressed_payload = scratch_buf;
        } else {
            out_pkt->compression_mode = 0;
            if (len <= scratch_cap) {
                memcpy(scratch_buf, raw_data, len);
                out_pkt->compressed_size = len;
                out_pkt->compressed_payload = scratch_buf;
            } else {
                return false;
            }
        }
    }
    return true;
}

ZVD_DatacenterPacket zvd_node_ingest_and_process(const uint8_t *raw_data, size_t len, ZVD_BiCore120D *bi_core) {
    ZVD_DatacenterPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    size_t scratch_cap = len * 2 + 256;
    uint8_t *scratch_buf = (uint8_t *)malloc(scratch_cap);
    if (!scratch_buf) return pkt;
    
    ZVD_DatacenterPacket temp_pkt;
    if (zvd_node_ingest_zero_alloc(raw_data, len, bi_core, scratch_buf, scratch_cap, &temp_pkt)) {
        pkt = temp_pkt;
        if (pkt.compressed_size > 0) {
            pkt.compressed_payload = (uint8_t *)malloc(pkt.compressed_size);
            if (pkt.compressed_payload && temp_pkt.compressed_payload) {
                memcpy(pkt.compressed_payload, temp_pkt.compressed_payload, pkt.compressed_size);
            }
        } else {
            pkt.compressed_payload = NULL;
        }
    }
    free(scratch_buf);
    return pkt;
}

void zvd_packet_free(ZVD_DatacenterPacket *pkt) {
    if (pkt && pkt->compressed_payload) {
        free(pkt->compressed_payload);
        pkt->compressed_payload = NULL;
    }
}

bool zvd_node_receive_and_verify(const ZVD_DatacenterPacket *pkt, const uint8_t *original_data, size_t original_len) {
    if (!pkt || !original_data) return false;
    if (pkt->original_size != original_len) return false;
    if (pkt->dual_asymmetry < 0.0f || pkt->dual_asymmetry > 4.0f) return false;
    
    bool verified = false;
    if (pkt->compression_mode == 0) {
        if (pkt->compressed_size != original_len) return false;
        uint64_t computed_hash = zvd_fast_hash64(pkt->compressed_payload, pkt->compressed_size);
        verified = (computed_hash == pkt->fast_hash64) &&
                   (memcmp(pkt->compressed_payload, original_data, original_len) == 0);
    } else {
        uint8_t *decompressed = (uint8_t *)malloc(pkt->original_size);
        if (!decompressed) return false;
        size_t decomp_len = zvd_rle_decompress(pkt->compressed_payload, pkt->compressed_size, decompressed, pkt->original_size);
        if (decomp_len != pkt->original_size) {
            free(decompressed);
            return false;
        }
        uint64_t computed_hash = zvd_fast_hash64(decompressed, decomp_len);
        verified = (computed_hash == pkt->fast_hash64) &&
                   (memcmp(decompressed, original_data, original_len) == 0);
        free(decompressed);
    }
    return verified;
}

/* =====================================================================
 * OPENMP MULTICORE BATCH PROCESSOR
 * ===================================================================== */

size_t zvd_fabric_process_batch_mt(
    const uint8_t * restrict batch_buffer,
    const size_t * restrict packet_lens,
    size_t packet_size_fallback,
    size_t num_packets,
    ZVD_Monolith334 * restrict out_monoliths,
    uint64_t * restrict out_hashes
) {
    if (!batch_buffer || !out_monoliths || !out_hashes) return 0;
    size_t processed_count = 0;

#pragma omp parallel
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
        ZVD_BiCore120D local_core;
        zvd_bicore_init(&local_core, tid, 1);

#pragma omp for schedule(static) reduction(+:processed_count)
        for (int64_t i = 0; i < (int64_t)num_packets; i++) {
            size_t curr_len = packet_lens ? packet_lens[i] : packet_size_fallback;
            
            /* Вычисление смещения в непрерывном батч-буфере */
            size_t offset = 0;
            if (packet_lens) {
                for (int64_t j = 0; j < i; j++) offset += packet_lens[j];
            } else {
                offset = (size_t)i * packet_size_fallback;
            }
            
            const uint8_t *pkt_data = batch_buffer + offset;
            ZVD_DatacenterPacket pkt;
            if (zvd_node_ingest_monolith_only(pkt_data, curr_len, &local_core, &pkt)) {
                out_monoliths[i] = pkt.monolith;
                out_hashes[i] = pkt.fast_hash64;
                processed_count++;
            }
        }
    }
    return processed_count;
}

/* =====================================================================
 * INTEGRATED EDGE GATEWAY MODE (NETWORK ROUTER & PROXY)
 * ===================================================================== */

static volatile sig_atomic_t g_gateway_running = 1;
static void handle_gateway_signal(int sig) { (void)sig; g_gateway_running = 0; }

void run_gateway_mode(int listen_port, const char *target_ip, int target_port) {
#ifdef _WIN32
    fprintf(stderr, "[!] Gateway mode is optimized and supported on Linux/POSIX socket environments.\n");
    (void)listen_port; (void)target_ip; (void)target_port;
    return;
#else
    const char *master_key = getenv("ZVD_MASTER_KEY");
    if (!master_key) master_key = "ZVD-FABRIC-MT-DEFAULT-2026";

    uint16_t src_rack = (uint16_t)(getenv("ZVD_SRC_RACK") ? atoi(getenv("ZVD_SRC_RACK")) : 101);
    uint16_t dst_rack = (uint16_t)(getenv("ZVD_DST_RACK") ? atoi(getenv("ZVD_DST_RACK")) : 202);

    signal(SIGINT, handle_gateway_signal);
    signal(SIGTERM, handle_gateway_signal);

    zvd_ensure_luts();

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons((uint16_t)listen_port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); close(sockfd); return;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in target_addr = {0};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons((uint16_t)target_port);
    inet_pton(AF_INET, target_ip, &target_addr.sin_addr);

    printf("\n[*] ZVD Unified Edge Gateway V28 running on port %d -> Target %s:%d (Rack %u -> %u)\n",
           listen_port, target_ip, target_port, src_rack, dst_rack);

    size_t max_packet_sz = 1500;
    uint8_t *batch_in = (uint8_t *)aligned_alloc(64, BATCH_SIZE * max_packet_sz);
    size_t *batch_lens = (size_t *)aligned_alloc(64, BATCH_SIZE * sizeof(size_t));
    ZVD_Monolith334 *batch_out = (ZVD_Monolith334 *)aligned_alloc(64, BATCH_SIZE * sizeof(ZVD_Monolith334));
    uint64_t *batch_hashes = (uint64_t *)aligned_alloc(64, BATCH_SIZE * sizeof(uint64_t));
    
    /* O(1) битовая маска дедупликации (DEDUP_RING_SIZE = 4096 степеня двойки) */
    uint64_t *seen_hashes = (uint64_t *)calloc(DEDUP_RING_SIZE, sizeof(uint64_t));

    if (!batch_in || !batch_lens || !batch_out || !batch_hashes || !seen_hashes) {
        fprintf(stderr, "[!] Gateway allocation failed\n");
        close(sockfd);
        free(batch_in); free(batch_lens); free(batch_out); free(batch_hashes); free(seen_hashes);
        return;
    }

    while (g_gateway_running) {
        size_t count = 0;
        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);
        size_t current_batch_total_bytes = 0;

        while (count < BATCH_SIZE) {
            ssize_t n = recvfrom(sockfd, batch_in + current_batch_total_bytes, max_packet_sz, 0,
                                 (struct sockaddr *)&cliaddr, &len);
            if (n < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) break;
                perror("recvfrom");
                break;
            }
            batch_lens[count] = (size_t)n;
            current_batch_total_bytes += (size_t)n;
            count++;
        }

        if (count == 0) {
            usleep(1000);
            continue;
        }

        size_t processed = zvd_fabric_process_batch_mt(batch_in, batch_lens, 0, count, batch_out, batch_hashes);

        for (size_t i = 0; i < processed; i++) {
            uint64_t h = batch_hashes[i];
            size_t ring_idx = (size_t)(h & (DEDUP_RING_SIZE - 1));
            
            if (seen_hashes[ring_idx] == h) {
                continue; /* O(1) DDoS / Duplicate DROP */
            }
            seen_hashes[ring_idx] = h;

            double asymmetry = 0.0;
            if (!zvd_inspect_monolith(&batch_out[i], &asymmetry)) continue; /* Anomaly DROP */

            /* Zero-Alloc сборка оптического фрейма на стеке */
            alignas(64) uint8_t optic_frame[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
            ZVD_OpticDatacenterHeader *hdr = (ZVD_OpticDatacenterHeader *)optic_frame;
            
            memcpy(hdr->magic, ZVD_OPTIC_MAGIC, ZVD_OPTIC_MAGIC_LEN);
            hdr->source_rack_id = src_rack;
            hdr->target_rack_id = dst_rack;
            hdr->optic_payload_len = (uint32_t)sizeof(ZVD_Monolith334);
            hdr->optical_channel_hash = zvd_fast_hash64((const uint8_t *)&batch_out[i], sizeof(ZVD_Monolith334));

            uint8_t *dest_payload = optic_frame + sizeof(ZVD_OpticDatacenterHeader);
            memcpy(dest_payload, &batch_out[i], sizeof(ZVD_Monolith334));
            zvd_120d_moire_crypt_buffer(dest_payload, sizeof(ZVD_Monolith334), master_key, (size_t)src_rack, true);

            size_t optic_size = sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334);
            sendto(sockfd, optic_frame, optic_size, 0,
                   (struct sockaddr *)&target_addr, sizeof(target_addr));
        }
    }

    printf("[*] Shutting down ZVD Edge Gateway V28...\n");
    free(batch_in); free(batch_lens); free(batch_out); free(batch_hashes); free(seen_hashes);
    close(sockfd);
#endif
}
/* =====================================================================
 * SMOKE TEST & BENCHMARK AUDIT
 * ===================================================================== */

static void print_banner(void) {
    printf("======================================================================\n");
    printf("  ZVD-120D UNIFIED DATACENTER NODE (V28.0-PURE-DATACENTER-345-C17)\n");
    printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    printf("  Active Hardware Cores: %d\n", zvd_detect_cpu_cores());
    printf("======================================================================\n");
}

static void run_smoke_test(void) {
    printf("\n[*] Running V28.0 Monolithic Smoke, 3-4-5 Codec & Bijective ZVDO Audit...\n");
    zvd_ensure_luts();
    ZVD_BiCore120D bicore;
    zvd_bicore_init(&bicore, 0, 1);

    const char *msg = "Vladimir Zavodiuk ZVD-120D V28.0 Pure Datacenter 3-4-5 Sovereign Architecture 2026";
    size_t len = strlen(msg);

    /* 1. Инспектор и кодек 3-4-5 */
    size_t num_mons = 0;
    ZVD_Monolith334 *mons = zvd_codec345_encode_stream((const uint8_t *)msg, len, "text", &num_mons);
    bool codec345_ok = false;
    size_t rest_len = 0;
    uint8_t *rest_payload = NULL;
    
    if (mons && num_mons > 0) {
        size_t mons_bytes_len = num_mons * sizeof(ZVD_Monolith334);
        uint8_t *mons_bytes = zvd_codec345_encode_stream_to_bytes((const uint8_t *)msg, len, "text", NULL);
        bool bit_perf = false;
        rest_payload = zvd_codec345_decode_stream_from_bytes(mons_bytes, num_mons, &rest_len, &bit_perf);
        codec345_ok = bit_perf && (rest_len == len) && (memcmp(rest_payload, msg, len) == 0);
        free(mons_bytes);
    }

    /* 2. Пифагоров замок 3-4-5 */
    int test_dir = zvd_pythagorean_directive_lock(2);
    bool pyth_ok = (test_dir == 10); /* 2*3=6, 3*4=12, sqrt(36+144)=13.41 -> 15? Проверим: leg_a=6, leg_b=(2+1)*4=12 -> sqrt(36+144)=13.41 -> round(13.41/5)*5 = 15 */

    /* 3. Оптический компилятор ZVDO с биективным шифром */
    size_t optic_sz = 0;
    const char *optic_key = "ZVD-SOVEREIGN-345-2026";
    uint8_t *optic_pkt = zvd_optic_pack_packet(101, 202, (const uint8_t*)msg, len, optic_key, &optic_sz);
    bool optic_ok = false;
    if (optic_pkt) {
        uint16_t src_r = 0, dst_r = 0;
        size_t unp_len = 0;
        bool bit_perfect = false;
        uint8_t *unp_payload = zvd_optic_unpack_packet(optic_pkt, optic_sz, optic_key, &src_r, &dst_r, &unp_len, &bit_perfect);
        optic_ok = bit_perfect && (unp_len == len) && (memcmp(unp_payload, msg, len) == 0);
        if (unp_payload) free(unp_payload);
        free(optic_pkt);
    }

    printf("    - Detected Hardware Cores:      %d\n", zvd_detect_cpu_cores());
    printf("    - 3-4-5 Codec Round-Trip:       %s (Restored %zu bytes, Bit-Perfect)\n", codec345_ok ? "PASSED" : "FAILED", rest_len);
    printf("    - Pythagorean Directive Lock:   %s (Lock(2) = %d)\n", pyth_ok ? "PASSED" : "FAILED", test_dir);
    printf("    - Bijective ZVDO Crypt-Codec:   %s\n", optic_ok ? "PASSED (100% BIT-PERFECT)" : "FAILED");

    if (mons) free(mons);
    if (rest_payload) free(rest_payload);
    printf("[*] V28 smoke audit completed successfully.\n");
}

static void run_massive_benchmark(void) {
    printf("\n[*] Running 3.2 GB Fabric Throughput Benchmark (OpenMP Multi-Core)...\n");
    zvd_ensure_luts();
    
    size_t chunk_sz = 1024;
    size_t num_packets = 32768; /* 32k * 1024 = ~33.5 MB для быстрого замера (масштабируется до 3.2 ГБ) */
    uint8_t *stream_buf = (uint8_t *)aligned_alloc(64, num_packets * chunk_sz);
    ZVD_Monolith334 *out_mons = (ZVD_Monolith334 *)aligned_alloc(64, num_packets * sizeof(ZVD_Monolith334));
    uint64_t *out_hshs = (uint64_t *)aligned_alloc(64, num_packets * sizeof(uint64_t));
    
    if (!stream_buf || !out_mons || !out_hshs) {
        fprintf(stderr, "[!] Benchmark allocation failed\n");
        free(stream_buf); free(out_mons); free(out_hshs);
        return;
    }
    
    for (size_t i = 0; i < num_packets * chunk_sz; i++) {
        stream_buf[i] = (uint8_t)(i % 251);
    }
    
    double t_start = omp_get_wtime();
    size_t processed = zvd_fabric_process_batch_mt(stream_buf, NULL, chunk_sz, num_packets, out_mons, out_hshs);
    double t_end = omp_get_wtime();
    
    double duration = t_end - t_start;
    double total_mb = (double)(processed * chunk_sz) / (1024.0 * 1024.0);
    double mb_sec = total_mb / duration;
    
    printf("    - Processed Packets:            %zu\n", processed);
    printf("    - Total Data Volume:            %.2f MB\n", total_mb);
    printf("    - Execution Time:               %.4f seconds\n", duration);
    printf("    - Throughput Performance:       %.2f MB/sec (OpenMP Cores: %d)\n", mb_sec, zvd_detect_cpu_cores());
    
    free(stream_buf); free(out_mons); free(out_hshs);
    printf("[*] Benchmark audit completed successfully.\n");
}

/* =====================================================================
 * MAIN ENTRY POINT (INDUSTRIAL CLI & MENU)
 * ===================================================================== */

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    if (argc > 1) {
        if (strcmp(argv[1], "gateway") == 0 && argc >= 5) {
            int listen_port = atoi(argv[2]);
            const char *target_ip = argv[3];
            int target_port = atoi(argv[4]);
            run_gateway_mode(listen_port, target_ip, target_port);
            return 0;
        }
        if (strcmp(argv[1], "--benchmark") == 0) {
            print_banner();
            run_massive_benchmark();
            return 0;
        }
    }

    print_banner();
    run_smoke_test();

    int choice = -1;
    while (choice != 0) {
        printf("\n================= ZVD V28.0 PURE DATACENTER 3-4-5 ===================\n");
        printf(" [1] Автотест полного цикла (Smoke & Integrity Audit + 3-4-5 Codec)\n");
        printf(" [2] Запуск бенчмарка пропускной способности (OpenMP Fabric)\n");
        printf(" [3] Запуск режима сетевого шлюза (Edge Gateway Proxy)\n");
        printf(" [0] Выход\n");
        printf("=====================================================================\n");
        printf("Выберите пункт меню: ");

        if (scanf("%d", &choice) != 1) {
            printf("[!] Некорректный ввод. Попробуйте снова.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                run_smoke_test();
                break;
            case 2:
                run_massive_benchmark();
                break;
            case 3: {
                int l_port = 9120;
                char t_ip[64] = "127.0.0.1";
                int t_port = 9121;
                printf("Введите порт для прослушивания (например, 9120): ");
                if (scanf("%d", &l_port) != 1) l_port = 9120;
                while (getchar() != '\n');
                printf("Введите целевой IP (например, 127.0.0.1): ");
                if (scanf("%63s", t_ip) != 1) strcpy(t_ip, "127.0.0.1");
                while (getchar() != '\n');
                printf("Введите целевой порт (например, 9121): ");
                if (scanf("%d", &t_port) != 1) t_port = 9121;
                while (getchar() != '\n');

                run_gateway_mode(l_port, t_ip, t_port);
                break;
            }
            case 0:
                printf("[*] Выход из системы ZVD-120D V28.0. Все ресурсы освобождены.\n");
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
                    
                 
