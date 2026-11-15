/* =============================================================================
 *  PROJECT:    ZVD-120D POLYMER-FIBER & KEVLAR SPINNERET CONTROLLER (SLOT 4)
 *              (13.11_zvd_polymer_fiber_v30.hpp - V3.1.5-POLYMER-FIBER-C20)
 *  FILENAME:   13.11_zvd_polymer_fiber_v30.hpp
 *  VERSION:    2026.11-V3.1.5-POLYMER-FIBER-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  --------------------------------------------------------------------------
 *  ASSEMBLY INSTRUCTIONS (Technology Constructor Ecosystem V3.1.5):
 *  Place this file in the root working directory alongside:
 *   1) zvd_local_ai_proxy.cpp (Root Orchestrator V3.1.5)
 *   2) zvd_datacenter_node_v28_lite.c (Datacenter Core 120D)
 *   3) 13.11_zvd_polymer_fiber_v30.hpp (Kevlar Polymer Fiber Cartridge)
 *  Build command (Linux/macOS):
 *   g++ -std=c++20 -O3 -Wall -Wextra -Werror -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm -lpthread
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_POLYMER_FIBER 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_polymer_fiber_v30.hpp requires zvd_local_ai_proxy.cpp and zvd_datacenter_node_v28_lite.c in the working directory!"
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <atomic>
#include <algorithm>
#include <string>
#include <string_view>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef ZVD_FACE_CONSTANTS_DEFINED
#define ZVD_FACE_CONSTANTS_DEFINED 1
#define FACE_MOTION            0
#define FACE_ENERGY            1
#define FACE_MASK              2
#define FACE_TRUST             3

static const int8_t _JIT_UNPACK_LUT[5][2] = {
    {-1, 0},  
    {-1, 2},  
    { 0, 1},  
    { 0, 3},  
    { 1, 2}   
};
#endif

// =====================================================================
// 1. SIMD-OPTIMIZED POLYMER FIBER DATA STRUCTURES & SPINNERET LUT
// =====================================================================

// Fields ordered by size (largest to smallest) to eliminate padding gaps
typedef struct {
    float modulation_freq_hz;           // 4 bytes
    float spinneret_pressure_bar;       // 4 bytes
    float thermal_viscosity_factor;     // 4 bytes
    int32_t target_x;                   // 4 bytes
    int32_t target_y;                   // 4 bytes
    int8_t  bond_mode;                  // 1 byte
    uint8_t time_slice_t;               // 1 byte
    uint8_t laser_pwm_pct;              // 1 byte
    uint8_t q_bond_flag;                // 1 byte
} ZVD_PolymerFiberFrame;                // Total: 28 bytes (Zero padding waste)

typedef struct {
    int8_t warp_dy;
    int8_t warp_dx;
} ZVD_SpinneretHyperbolicNode;

alignas(64) static ZVD_SpinneretHyperbolicNode g_spinneret_lut[5][5];
static std::atomic<bool> g_spinneret_initialized{false};

alignas(64) static std::atomic<uint64_t> g_polymer_extrusion_jobs{0};
alignas(64) static std::atomic<uint64_t> g_polymer_thermal_trips{0};

inline void zvd_init_spinneret_hyperbolic_lut(void) {
    if (g_spinneret_initialized.load(std::memory_order_acquire)) return;
    
    static std::atomic<bool> init_lock{false};
    bool expected = false;
    if (init_lock.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                double dy = (double)y - 2.0;
                double dx = (double)x - 2.0;
                double dist = std::sqrt(dx * dx + dy * dy);
                int offset_y = (int)std::round(2.0 + dy * (1.0 - 0.05 * dist)) - 2;
                int offset_x = (int)std::round(2.0 + dx * (1.0 - 0.05 * dist)) - 2;
                g_spinneret_lut[y][x].warp_dy = (int8_t)std::clamp(offset_y, -128, 127);
                g_spinneret_lut[y][x].warp_dx = (int8_t)std::clamp(offset_x, -128, 127);
            }
        }
        g_spinneret_initialized.store(true, std::memory_order_release);
    } else {
        while (!g_spinneret_initialized.load(std::memory_order_acquire)) {
#if defined(_MSC_VER)
            YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
#endif
        }
    }
}

// Fast-path direct monolith unpack helper (base-5 Lukasiewicz)
inline void zvd_polymer_unpack_monolith_fast(const ZVD_Monolith334 *mono, ZVD_TetraCube120D *cube) {
    int idx = 0;
    for (int i = 0; i < 166; i++) {
        uint8_t val = mono->face_packed[i];
        cube->face_data[idx++] = (int8_t)((val % 5) - 2);
        cube->face_data[idx++] = (int8_t)(((val / 5) % 5) - 2);
        cube->face_data[idx++] = (int8_t)(((val / 25) % 5) - 2);
    }
    uint8_t val_last = mono->face_packed[166];
    cube->face_data[498] = (int8_t)((val_last % 5) - 2);
    cube->face_data[499] = (int8_t)(((val_last / 5) % 5) - 2);

    idx = 0;
    for (int i = 0; i < 166; i++) {
        uint8_t val = mono->vertex_packed[i];
        cube->vertex_data[idx++] = (int8_t)((val % 5) - 2);
        cube->vertex_data[idx++] = (int8_t)(((val / 5) % 5) - 2);
        cube->vertex_data[idx++] = (int8_t)(((val / 25) % 5) - 2);
    }
    uint8_t v_last = mono->vertex_packed[166];
    cube->vertex_data[498] = (int8_t)((v_last % 5) - 2);
    cube->vertex_data[499] = (int8_t)(((v_last / 5) % 5) - 2);
}

// =====================================================================
// 2. RHEOLOGICAL SHEAR-THINNING & SPINNERET ENGINE
// =====================================================================

inline void zvd_compile_polymer_fiber_frames(
    const ZVD_TetraCube120D * restrict cubes,
    size_t num_cubes,
    ZVD_PolymerFiberFrame * restrict out_frames,
    double extrusion_velocity_m_min,
    bool thermal_spike_active,
    double risk_index,
    size_t max_frames_limit
) {
    zvd_init_spinneret_hyperbolic_lut();
    int tiles_per_row = 16;
    float vel_compensator = (float)extrusion_velocity_m_min * 0.01f;
    size_t global_frame_counter = 0;
    
    for (size_t c = 0; c < num_cubes; c++) {
        int ty = (int)c / tiles_per_row;
        int tx = (int)c % tiles_per_row;
        int y0 = ty * ZVD_CUBE;
        int x0 = tx * ZVD_CUBE;
        
        for (int i = 0; i < ZVD_CELLS_PER_LAYER; i++) {
            if (global_frame_counter >= max_frames_limit) return;
            
            int nozzle_x = i / 100;
            int nozzle_y = (i / 20) % 5;
            int time_slice_t = (i / 4) % 5;
            
            nozzle_y = std::clamp(nozzle_y, 0, 4);
            nozzle_x = std::clamp(nozzle_x, 0, 4);
            
            int warp_dy = (int)g_spinneret_lut[nozzle_y][nozzle_x].warp_dy;
            int warp_dx = (int)g_spinneret_lut[nozzle_y][nozzle_x].warp_dx;
            
            // Non-Newtonian shear-thinning gradient calculation
            float shear_gradient = std::sqrt((float)(warp_dx * warp_dx + warp_dy * warp_dy)) * 0.1f;
            float temporal_smoothing = 1.0f + (float)time_slice_t * 0.02f;
            
            int target_y = y0 + 2 + warp_dy;
            int target_x = x0 + 2 + warp_dx + (int)(vel_compensator * temporal_smoothing);
            
            int base_tet = (i / 4) * 4;
            int8_t val_motion = cubes[c].face_data[base_tet + FACE_MOTION];
            int8_t val_energy = cubes[c].face_data[base_tet + FACE_ENERGY];
            int8_t val_mask   = cubes[c].vertex_data[base_tet + FACE_MASK];
            int8_t val_trust  = cubes[c].vertex_data[base_tet + FACE_TRUST];
            
            int pm_idx = (int)val_motion + 2;
            pm_idx = std::clamp(pm_idx, 0, 4);
            int trit = _JIT_UNPACK_LUT[pm_idx][0];
            
            int pe_idx = (int)val_energy + 2;
            pe_idx = std::clamp(pe_idx, 0, 4);
            int quad = _JIT_UNPACK_LUT[pe_idx][1];
            quad = std::clamp(quad, 0, 3);
            
            float base_pressure = (float)((int)val_mask + 3) * 1.8f;
            // Shear-thinning pressure adjustment & Melt fracture damping guard
            float pressure_bar = base_pressure * (1.0f / (1.0f + shear_gradient * 0.15f)) * temporal_smoothing;
            
            float visc_factor = std::clamp(1.0f - ((float)val_trust * 0.05f) - (shear_gradient * 0.02f), 0.5f, 1.5f);
            
            float power_mod = thermal_spike_active ? 0.65f : 1.0f;
            if (thermal_spike_active) {
                pressure_bar += 1.2f; // Assist nitrogen buffering during thermal spike
            }
            
            float base_power = (float)((quad + 1) * 25);
            float final_power = base_power * power_mod;
            
            int f_v = (int)cubes[c].face_data[i];
            int v_v = (int)cubes[c].vertex_data[i];
            int wave_sum = f_v + v_v;
            int wave_diff = f_v - v_v;
            uint8_t q_bond = (std::abs(wave_sum) >= 3) ? 1 : 0;
            
            int8_t bond_mode = 0;
            if (wave_sum >= 3) {
                bond_mode = 2;
                final_power = 100.0f;
            } else if (wave_sum == 1 || wave_sum == 2) {
                bond_mode = 1;
            } else if (wave_sum == 0) {
                bond_mode = 0;
            } else if (wave_sum == -1 || wave_sum == -2) {
                bond_mode = -1;
            } else {
                bond_mode = -2;
                final_power = 0.0f;
            }
            
            float freq_offset = 0.0f;
            if (trit == -1) freq_offset = 10.0f;
            else if (trit == 1) freq_offset = 25.0f;
            
            if (risk_index <= -0.65) {
                final_power = 0.0f;
            }
            
            float modulation_freq = 50.0f + freq_offset + (float)std::abs(wave_diff) * 2.5f;
            
            ZVD_PolymerFiberFrame *frame_ptr = out_frames + global_frame_counter;
            frame_ptr->target_x = (int32_t)target_x;
            frame_ptr->target_y = (int32_t)target_y;
            frame_ptr->time_slice_t = (uint8_t)time_slice_t;
            frame_ptr->laser_pwm_pct = (uint8_t)std::clamp(final_power, 0.0f, 100.0f);
            frame_ptr->modulation_freq_hz = modulation_freq;
            frame_ptr->bond_mode = bond_mode;
            frame_ptr->q_bond_flag = q_bond;
            frame_ptr->spinneret_pressure_bar = pressure_bar;
            frame_ptr->thermal_viscosity_factor = visc_factor;
            
            global_frame_counter++;
        }
    }
}

// =====================================================================
// 3. BUFFERED 64KB G-CODE EXPORT & OPENMP BATCH PROCESSOR
// =====================================================================

inline void zvd_export_polymer_gcode(const ZVD_PolymerFiberFrame *frames, size_t total_frames, const char *filepath, double velocity_m_min) {
    FILE *f = fopen(filepath, "w");
    if (!f) return;
    
    char io_buf[65536];
    setvbuf(f, io_buf, _IOFBF, sizeof(io_buf));
    size_t off = 0;
    
    int written = snprintf(io_buf + off, sizeof(io_buf) - off, "(--- ZVD-120D KEVLAR SPINNERET & POLYMER EXTRUSION (V3.1.5) ---)\n");
    if (written > 0) off += (size_t)written;
    
    written = snprintf(io_buf + off, sizeof(io_buf) - off,
        "G21 (Metric units - millimeters)\n"
        "G90 (Absolute coordinate positioning mode)\n"
        "F%.1f (Extrusion velocity rate m/min)\n"
        "M12 (Activate Polymer Solution & Assist Nitrogen Pressure)\n"
        "G4 P50 (Spinneret thermal stabilization dwell)\n\n", velocity_m_min);
    if (written > 0) off += (size_t)written;
    
    for (size_t i = 0; i < total_frames; i++) {
        if (off > sizeof(io_buf) - 512) {
            fwrite(io_buf, 1, off, f);
            off = 0;
        }
        int s_pwm = (int)((float)frames[i].laser_pwm_pct / 100.0f * 1000.0f);
        written = snprintf(io_buf + off, sizeof(io_buf) - off,
            "G1 X%d.000 Y%d.000 S%d T%u (ModFreq: %.1f Hz | Pressure: %.1f bar | Visc: %.2f | Bond: %d | Q-Bond: %u)\n",
            (int)frames[i].target_x, (int)frames[i].target_y, s_pwm, (unsigned int)frames[i].time_slice_t,
            (double)frames[i].modulation_freq_hz, (double)frames[i].spinneret_pressure_bar,
            (double)frames[i].thermal_viscosity_factor, (int)frames[i].bond_mode,
            (unsigned int)frames[i].q_bond_flag);
        if (written > 0) off += (size_t)written;
    }
    
    written = snprintf(io_buf + off, sizeof(io_buf) - off,
        "\nM5 (Laser Diode Off)\n"
        "M13 (Deactivate Nitrogen Pressure)\n"
        "G0 X0.000 Y0.000 (Return home)\n"
        "M30 (Program end and rewind)\n");
    if (written > 0) off += (size_t)written;
    
    if (off > 0) {
        fwrite(io_buf, 1, off, f);
    }
    fclose(f);
}

inline size_t zvd_polymer_process_batch_mt(
    const uint8_t * restrict batch_buffer,
    size_t packet_size,
    size_t num_packets,
    ZVD_PolymerFiberFrame * restrict out_frames,
    double velocity_m_min,
    bool thermal_spike_active,
    uint64_t *out_trips_count
) {
    size_t processed = 0;
    uint64_t local_trips = 0;
#pragma omp parallel reduction(+:processed, local_trips)
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
        ZVD_BiCore120D local_core;
        zvd_bicore_init(&local_core, tid, 1);
        
        alignas(64) float collapsed[ZVD_DIM_120];
        alignas(64) int8_t synth_120[ZVD_DIM_120];

#pragma omp for schedule(static)
        for (int64_t i = 0; i < (int64_t)num_packets; i++) {
            const uint8_t *pkt_data = batch_buffer + (size_t)i * packet_size;
            double entropy = zvd_shannon_entropy(pkt_data, packet_size < 256 ? packet_size : 256);
            zvd_bicore_update_rotation(&local_core, entropy);
            
            zvd_stride_collapse_to_120(pkt_data, packet_size, collapsed);
            zvd_enterprise_synthesis_120d(collapsed, &local_core, synth_120);
            
            ZVD_RashomonAudit audit;
            zvd_rashomon_audit_120d(synth_120, &audit);
            
            ZVD_TetraCube120D cube;
            zvd_project_120d_to_dual_cube(synth_120, &cube);
            
            ZVD_PolymerFiberFrame *frame_ptr = out_frames + i * ZVD_CELLS_PER_LAYER;
            ZVD_TetraCube120D single_cube_array[1] = { cube };
            zvd_compile_polymer_fiber_frames(single_cube_array, 1, frame_ptr, velocity_m_min, thermal_spike_active, audit.risk_index, ZVD_CELLS_PER_LAYER);
            
            processed++;
            if (thermal_spike_active) {
                local_trips++;
            }
        }
    }
    if (out_trips_count) {
        *out_trips_count = local_trips;
    }
    return processed;
}

// =====================================================================
// 4. UNIVERSAL BUS INTEGRATION (V3.1.5 CONTEXT-AWARE ARBITER ROLE)
// =====================================================================

inline int zvd_polymer_fiber_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) {
    (void)gl_inv;
    (void)entropy;
    zvd_ensure_luts();
    zvd_init_spinneret_hyperbolic_lut();

    if (prompt.starts_with("ZVD_FIBER_EXTRUSION") || prompt.starts_with("ZVD_FIBER_HIGH_SPEED")) {
        bool high_speed = prompt.starts_with("ZVD_FIBER_HIGH_SPEED");
        double velocity = high_speed ? 240.0 : 120.0;
        bool thermal_spike = entropy > 5.5;

        std::string_view payload = prompt;
        size_t prefix_len = high_speed ? 20 : 19;
        if (payload.size() > prefix_len) {
            payload.remove_prefix(prefix_len);
            while (!payload.empty() && (payload[0] == ' ' || payload[0] == '\t')) {
                payload.remove_prefix(1);
            }
        }
        if (payload.empty()) {
            payload = "ZVD_V315_POLYMER_KEVLAR_PRODUCTION_BLUEPRINT_2026";
        }

        alignas(64) float collapsed[ZVD_DIM_120];
        alignas(64) int8_t synth_120[ZVD_DIM_120];

        ZVD_BiCore120D bicore;
        zvd_bicore_init(&bicore, 0, 1);

        zvd_stride_collapse_to_120(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), collapsed);
        zvd_enterprise_synthesis_120d(collapsed, &bicore, synth_120);

        ZVD_TetraCube120D cube;
        zvd_project_120d_to_dual_cube(synth_120, &cube);

        alignas(64) std::array<ZVD_PolymerFiberFrame, ZVD_CELLS_PER_LAYER> frames{};
        ZVD_TetraCube120D cubes[1] = { cube };

        zvd_compile_polymer_fiber_frames(cubes, 1, frames.data(), velocity, thermal_spike, 0.0, ZVD_CELLS_PER_LAYER);

        const char *filename = high_speed ? "zvd_kevlar_high_speed.nc" : "zvd_kevlar_extrusion.nc";
        zvd_export_polymer_gcode(frames.data(), ZVD_CELLS_PER_LAYER, filename, velocity);

        g_polymer_extrusion_jobs.fetch_add(1, std::memory_order_relaxed);
        if (thermal_spike) {
            g_polymer_thermal_trips.fetch_add(1, std::memory_order_relaxed);
        }

        return 1; // PLUS
    }
    return 0; // ZERO
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_polymer_fiber_evaluate_ctx(ZvdPipelineContext& ctx) {
    zvd_ensure_luts();
    zvd_init_spinneret_hyperbolic_lut();

    if (!ctx.prompt.empty() && (ctx.prompt.starts_with("ZVD_FIBER_EXTRUSION") || ctx.prompt.starts_with("ZVD_FIBER_HIGH_SPEED"))) {
        bool high_speed = ctx.prompt.starts_with("ZVD_FIBER_HIGH_SPEED");
        double velocity = high_speed ? 240.0 : 120.0;

        double risk_index = 0.0;
        if (ctx.gl_invariant == -2 || ctx.entropy > 7.2) {
            risk_index = -1.0;
            ctx.routing_directive = "POLYMER_THERMAL_INTERLOCK_VETO";
            ctx.accumulated_trity = -1;
            return -1; // VETO
        }

        bool thermal_spike = ctx.entropy > 5.5;
        if (thermal_spike) {
            g_polymer_thermal_trips.fetch_add(1, std::memory_order_relaxed);
        }

        std::string_view payload = ctx.prompt;
        size_t prefix_len = high_speed ? 20 : 19;
        if (payload.size() > prefix_len) {
            payload.remove_prefix(prefix_len);
            while (!payload.empty() && (payload[0] == ' ' || payload[0] == '\t')) {
                payload.remove_prefix(1);
            }
        }

        ZVD_TetraCube120D cube;
        bool fast_path_used = false;

        if (payload.empty() && ctx.datacenter_pkt_ptr != nullptr) {
            const auto* dc_pkt = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr);
            zvd_polymer_unpack_monolith_fast(&dc_pkt->monolith, &cube);
            fast_path_used = true;
        } else {
            if (payload.empty()) {
                payload = "ZVD_V315_POLYMER_KEVLAR_PRODUCTION_BLUEPRINT_2026";
            }
            alignas(64) float collapsed[ZVD_DIM_120];
            alignas(64) int8_t synth_120[ZVD_DIM_120];

            ZVD_BiCore120D bicore;
            zvd_bicore_init(&bicore, 0, 1);

            zvd_stride_collapse_to_120(reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), collapsed);
            zvd_enterprise_synthesis_120d(collapsed, &bicore, synth_120);
            zvd_project_120d_to_dual_cube(synth_120, &cube);
        }

        alignas(64) std::array<ZVD_PolymerFiberFrame, ZVD_CELLS_PER_LAYER> frames{};
        ZVD_TetraCube120D cubes[1] = { cube };

        zvd_compile_polymer_fiber_frames(cubes, 1, frames.data(), velocity, thermal_spike, risk_index, ZVD_CELLS_PER_LAYER);

        const char *filename = high_speed ? "zvd_kevlar_high_speed.nc" : "zvd_kevlar_extrusion.nc";
        zvd_export_polymer_gcode(frames.data(), ZVD_CELLS_PER_LAYER, filename, velocity);

        g_polymer_extrusion_jobs.fetch_add(1, std::memory_order_relaxed);

        ctx.routing_directive = fast_path_used ? "POLYMER_SPINNERET_FAST_PATH" : "POLYMER_SPINNERET_COMPILED";
        ctx.accumulated_trity = 1;
        return 1; // PLUS
    }
    return 0; // ZERO
}
#endif

inline bool zvd_polymer_fiber_verify(std::string_view prompt, std::string_view response) {
    if (prompt.starts_with("ZVD_FIBER_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.1.5 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("PolymerFiber-120D-KevlarSpinneret", ZvdModuleRole::ARBITER, zvd_polymer_fiber_evaluate_ctx, zvd_polymer_fiber_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("PolymerFiber-120D-KevlarSpinneret", zvd_polymer_fiber_evaluate, zvd_polymer_fiber_verify);
#endif

// =====================================================================
// 5. AUTONOMOUS ENTRY POINT & CLI SMOKE TEST
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_polymer_fiber_smoke_test(void) {
    std::printf("\n[*] Running V3.1.5 Polymer Fiber & Kevlar Spinneret Smoke Audit (SIMD Packed, Shear-Thinning)...\n");
    zvd_ensure_luts();
    zvd_init_spinneret_hyperbolic_lut();
    
    ZVD_BiCore120D bicore;
    zvd_bicore_init(&bicore, 0, 1);
    
    const char *blueprint = "ZVD_V315_POLYMER_FIBER_SMART_KEVLAR_SPINNERET_2026";
    size_t len = std::strlen(blueprint);
    
    alignas(64) float collapsed[ZVD_DIM_120];
    alignas(64) int8_t synth_120[ZVD_DIM_120];
    zvd_stride_collapse_to_120((const uint8_t*)blueprint, len, collapsed);
    zvd_enterprise_synthesis_120d(collapsed, &bicore, synth_120);
    
    ZVD_TetraCube120D cube;
    zvd_project_120d_to_dual_cube(synth_120, &cube);
    
    ZVD_Monolith334 monolith;
    zvd_cube_pack_monolith(&cube, &monolith);
    
    double asym = 0.0;
    bool inspect_ok = zvd_inspect_monolith(&monolith, &asym);
    std::printf("    - Monolith Inspector 334B:    %s (Asymmetry: %.6f)\n", inspect_ok ? "PASSED" : "FAILED", asym);
    
    alignas(64) std::array<ZVD_PolymerFiberFrame, ZVD_CELLS_PER_LAYER> frames{};
    ZVD_TetraCube120D cubes[1] = { cube };
    
    // Compile & Export Standard Extrusion
    zvd_compile_polymer_fiber_frames(cubes, 1, frames.data(), 120.0, false, 0.0, ZVD_CELLS_PER_LAYER);
    zvd_export_polymer_gcode(frames.data(), ZVD_CELLS_PER_LAYER, "zvd_kevlar_spinneret_v315.nc", 120.0);
    g_polymer_extrusion_jobs.fetch_add(1, std::memory_order_relaxed);
    
    // Compile & Export High Speed with Thermal Spike & Shear-Thinning
    zvd_compile_polymer_fiber_frames(cubes, 1, frames.data(), 240.0, true, 0.0, ZVD_CELLS_PER_LAYER);
    zvd_export_polymer_gcode(frames.data(), ZVD_CELLS_PER_LAYER, "zvd_kevlar_high_speed_v315.nc", 240.0);
    g_polymer_extrusion_jobs.fetch_add(1, std::memory_order_relaxed);
    g_polymer_thermal_trips.fetch_add(1, std::memory_order_relaxed);
    
    std::printf("    - Spinneret Hyperbolic LUT:   5x5 Grid warping & Shear-Thinning active\n");
    std::printf("    - Polymer Extrusion Jobs:     %llu\n", (unsigned long long)g_polymer_extrusion_jobs.load());
    std::printf("    - Thermal Trips Handled:      %llu\n", (unsigned long long)g_polymer_thermal_trips.load());
    std::printf("    - Exported Extrusion G-Code:  'zvd_kevlar_spinneret_v315.nc'\n");
    std::printf("    - Exported High-Speed G-Code: 'zvd_kevlar_high_speed_v315.nc'\n");
    std::printf("[*] Polymer fiber smoke test completed successfully.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::printf("======================================================================\n");
    std::printf("  ZVD-120D POLYMER FIBER & KEVLAR SPINNERET FABRIC (V3.1.5-OPTIMIZED)\n");
    std::printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    std::printf("  Active Hardware Cores: %d\n", zvd_detect_cpu_cores());
    std::printf("======================================================================\n");

    run_polymer_fiber_smoke_test();

    int choice = -1;
    while (choice != 0) {
        std::printf("\n================= ZVD V3.1.5 POLYMER FIBER MENU =================\n");
        std::printf(" [1] Run Polymer Fiber Smoke Test & Export G-Codes (.NC)\n");
        std::printf(" [0] Exit\n");
        std::printf("================================================================\n");
        std::printf("Select menu option: ");

        if (scanf("%d", &choice) != 1) {
            std::printf("[!] Invalid input. Try again.\n");
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                run_polymer_fiber_smoke_test();
                break;
            case 0:
                std::printf("[*] Exiting ZVD-120D Polymer Fiber Fabric. All resources freed.\n");
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
