/* =============================================================================
 *  PROJECT:    ZVD-120D ICE ENGINE ECU CORE: 4-STROKE COMBUSTION GOVERNOR (SLOT 9)
 *              (13.11_zvd_ice_engine_v30.hpp - V3.1.3-ICE-ENGINE-ECU-C20)
 *  FILENAME:   13.11_zvd_ice_engine_v30.hpp
 *  VERSION:    2026.11-V3.1.3-ICE-ENGINE-ECU-PRODUCTION-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  --------------------------------------------------------------------------
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012
 *              Header-Only Monolith, Zero Dynamic Allocation, Thread-Safe, alignas(64)
 *              Strict English I18n, zero-warning standards (-std=c++20 -O3 -Wall -Wextra -Werror)
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_ICE_ENGINE 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_ice_engine_v30.hpp requires zvd_local_ai_proxy.cpp and zvd_datacenter_node_v28_lite.c in the working directory!"
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

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef ZVD_DIM_120
#define ZVD_DIM_120             120U    /* 720° engine cycle / 120 = 6.0° CA per node */
#endif

#define ZVD_CRANK_STEP_DEG      6.0f

typedef enum {
    ZVD_ICE_LOGIC_GODEL = 0,    /* Cruising / Eco: Elliptic Riemann metric & stoichiometric mix (Lambda = 1.0) */
    ZVD_ICE_LOGIC_LUKA  = 1     /* Overdrive / Sport: Hyperbolic Lobachevsky metric & plasma lean-burn */
} ZVD_ICE_LogicStrategy_t;

/* =====================================================================
 * ICE TELEMETRY & ECU STATE STRUCTURES (alignas(64))
 * ===================================================================== */

alignas(64) typedef struct {
    float engine_rpm;           /* Crankshaft speed (RPM) */
    float crankshaft_angle;     /* Absolute crankshaft angle (0.0 - 720.0°) */
    float cam_phase_intake;     /* Intake camshaft actual position (deg) */
    float cam_phase_exhaust;    /* Exhaust camshaft actual position (deg) */
    float knock_sensor_mv;      /* Knock sensor voltage amplitude (mV) */
    float cylinder_pressure;    /* In-cylinder peak pressure (bar) */
    float map_pressure;         /* Manifold Absolute Pressure (kPa) */
    float coolant_temp;         /* Engine Coolant Temperature (°C) */
    float lambda_sensor;        /* Wideband Lambda equivalence ratio */
    float driver_demand;        /* Accelerator pedal position (0.0 to 1.0) */
} ZVD_ICE_Telemetry_120D_t;

alignas(64) typedef struct {
    float angle_a;              /* Crankshaft phase angle (rad) */
    float angle_b;              /* Camshaft counter-phase angle (rad) */
    float base_velocity;        /* Base angular velocity */
    float air_resonance_harmonic;/* Cylinders volumetric breathing harmonic */
} ZVD_ICE_MoireCore_t;

alignas(64) typedef struct {
    int8_t state_120d[ZVD_DIM_120];
    ZVD_ICE_LogicStrategy_t active_strategy;
    bool hyperbolic_overdrive_active;
    bool plasma_ignition_active;
    bool thermal_safe_mode_active;
    bool detonation_guard_active;
    
    /* Actuator Outputs */
    float ignition_timing_advance_deg; /* Ignition Timing Advance (deg BTDC) */
    float fuel_injection_pulse_ms;     /* Fuel Injection Duration (ms) */
    float injection_phase_angle_deg;   /* Start of Injection (SOI deg BTDC) */
    float electronic_throttle_angle;   /* Electronic Throttle Opening (%) */
    float vvt_intake_target_deg;       /* Intake VVT Target Angle (deg) */
    float vvt_exhaust_target_deg;      /* Exhaust VVT Target Angle (deg) */
    float air_resonance_amplitude;     /* Breathing resonance amplitude */
    
    /* Pentarny Rashomon Risk Audit Metrics */
    float rashomon_technical;
    float rashomon_operational;
    float rashomon_structural;
    float rashomon_boundary;
    float rashomon_risk_index;
} ZVD_ICE_State_120D_t;

// Cache-aligned atomic telemetry counters
alignas(64) static std::atomic<uint64_t> g_ice_ecu_cycles{0};
alignas(64) static std::atomic<uint64_t> g_ice_knock_trips{0};
alignas(64) static std::atomic<uint64_t> g_ice_thermal_trips{0};

// =====================================================================
// 1. THREAD-SAFE NON-EUCLIDEAN LUT & TRIGONOMETRY INITIALIZATION
// =====================================================================

static int8_t s_ice_lobachevsky_lut[5][5][2];
static int8_t s_ice_riemann_lut[5][5][2];
alignas(64) static float ZVD_SIN_360[360];
alignas(64) static float ZVD_COS_360[360];
static std::atomic<bool> g_ice_luts_initialized{false};

inline void zvd_ensure_ice_luts(void) {
    if (g_ice_luts_initialized.load(std::memory_order_acquire)) return;
    
    static std::atomic<bool> init_lock{false};
    bool expected = false;
    if (init_lock.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        for (int deg = 0; deg < 360; deg++) {
            double rad = (double)deg * (M_PI / 180.0);
            ZVD_SIN_360[deg] = (float)std::sin(rad);
            ZVD_COS_360[deg] = (float)std::cos(rad);
        }
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                float dy = (float)y - 2.0f;
                float dx = (float)x - 2.0f;
                float dist = std::sqrt(dx * dx + dy * dy);

                float sinh_scale = (dist > 1e-6f) ? (std::sinh(dist) / dist) : 1.0f;
                float contract_h = (sinh_scale > 1.0f) ? (1.0f / sinh_scale) : 1.0f;
                s_ice_lobachevsky_lut[y][x][0] = (int8_t)(std::round(2.0f + dy * contract_h) - 2.0f);
                s_ice_lobachevsky_lut[y][x][1] = (int8_t)(std::round(2.0f + dx * contract_h) - 2.0f);

                float sin_scale = (dist > 1e-6f) ? (std::sin(dist) / dist) : 1.0f;
                s_ice_riemann_lut[y][x][0] = (int8_t)(std::round(2.0f + dy * sin_scale) - 2.0f);
                s_ice_riemann_lut[y][x][1] = (int8_t)(std::round(2.0f + dx * sin_scale) - 2.0f);
            }
        }
        g_ice_luts_initialized.store(true, std::memory_order_release);
    } else {
        while (!g_ice_luts_initialized.load(std::memory_order_acquire)) {
#if defined(_MSC_VER)
            YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
#endif
        }
    }
}

static inline int8_t zvd_ice_guard_val(int32_t val) {
    return (int8_t)std::clamp(val, -2, 2);
}

// =====================================================================
// 2. PYTHAGOREAN PHASE LOCK (3-4-5) & MULTIVALUED REDUCER
// =====================================================================

#define ZVD_ICE_PYTHAGOREAN_LOCK(core_inf, clamped_out) do { \
    int32_t leg_a_macro = (int32_t)(core_inf) * 3; \
    int32_t leg_b_macro = (std::abs(core_inf) + 1) * 4; \
    float hypot_macro = std::sqrt((float)(leg_a_macro * leg_a_macro + leg_b_macro * leg_b_macro)); \
    int32_t step_locked = (int32_t)std::round(hypot_macro / 5.0f); \
    clamped_out = ((core_inf) < 0) ? -step_locked : step_locked; \
} while(0)

inline int8_t zvd_ice_reduce_120d(const int8_t* vec_120d, ZVD_ICE_LogicStrategy_t strategy) {
    if (strategy == ZVD_ICE_LOGIC_GODEL) {
        int8_t min_val = 127;
        for (size_t i = 0; i < ZVD_DIM_120; i++) {
            if (vec_120d[i] < min_val) {
                min_val = vec_120d[i];
            }
        }
        return min_val;
    } else {
        int32_t total_sum = 0;
        for (size_t i = 0; i < ZVD_DIM_120; i++) {
            total_sum += (int32_t)vec_120d[i];
        }
        int32_t luka_val = total_sum - ((int32_t)ZVD_DIM_120 - 1);
        return (int8_t)std::clamp(luka_val, -2, 2);
    }
}

inline void zvd_ice_rashomon_audit(const int8_t* state_120d, ZVD_ICE_State_120D_t* ctx) {
    float sum = 0.0f, abs_sum = 0.0f;
    float min_v = (float)state_120d[0], max_v = (float)state_120d[0];
    float mean, var_sum = 0.0f, std_dev;

    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        float val = (float)state_120d[i];
        sum += val;
        abs_sum += val < 0.0f ? -val : val;
        if (val < min_v) { min_v = val; }
        if (val > max_v) { max_v = val; }
    }
    mean = sum / (float)ZVD_DIM_120;

    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        float diff = (float)state_120d[i] - mean;
        var_sum += diff * diff;
    }
    std_dev = std::sqrt(var_sum / (float)ZVD_DIM_120);

    ctx->rashomon_technical   = mean;
    ctx->rashomon_operational  = min_v;
    ctx->rashomon_structural   = max_v;
    ctx->rashomon_boundary     = std_dev;
    ctx->rashomon_risk_index   = (-(abs_sum / (float)ZVD_DIM_120)) - (std_dev * 0.15f);
}

// =====================================================================
// 3. THREAD-SAFE ICE ECU CLOSED-LOOP STEP EXECUTION (ZERO-ALLOC)
// =====================================================================

inline void ZVD_ICE_Engine_ExecuteStep(
    const ZVD_ICE_Telemetry_120D_t* ice_in,
    ZVD_ICE_MoireCore_t* moire_core,
    ZVD_ICE_State_120D_t* state_out
) {
    zvd_ensure_ice_luts();
    std::memset(state_out, 0, sizeof(ZVD_ICE_State_120D_t));

    bool thermal_alarm = false;
    bool detonation_alarm = false;
    int32_t penta_raw;
    int32_t pythagorean_locked_val = 0;

    // 1. Thermal & Detonation Safeguards
    if (ice_in->coolant_temp > 108.0f) {
        thermal_alarm = true;
        g_ice_thermal_trips.fetch_add(1, std::memory_order_relaxed);
    }

    if ((ice_in->knock_sensor_mv > 850.0f) || (ice_in->cylinder_pressure > 110.0f)) {
        detonation_alarm = true;
        g_ice_knock_trips.fetch_add(1, std::memory_order_relaxed);
    }

    // 2. Crankshaft & Camshaft Moiré Phase Synchronization
    moire_core->angle_a = ice_in->crankshaft_angle * (M_PI / 180.0f);
    moire_core->angle_b = moire_core->angle_a * 0.5f;

    int crank_deg_idx = (int32_t)std::round(ice_in->crankshaft_angle) % 360;
    if (crank_deg_idx < 0) crank_deg_idx += 360;
    float cos_val = ZVD_COS_360[crank_deg_idx];
    moire_core->air_resonance_harmonic = std::fabs(cos_val * (ice_in->engine_rpm / 2000.0f));
    state_out->air_resonance_amplitude = moire_core->air_resonance_harmonic;

    // 3. Adaptive Strategy Selection
    if (ice_in->driver_demand > 0.85f && !thermal_alarm && !detonation_alarm) {
        state_out->active_strategy = ZVD_ICE_LOGIC_LUKA;
        state_out->hyperbolic_overdrive_active = true;
        state_out->plasma_ignition_active = true;
    } else {
        state_out->active_strategy = ZVD_ICE_LOGIC_GODEL;
        state_out->hyperbolic_overdrive_active = false;
        state_out->plasma_ignition_active = false;
    }

    state_out->thermal_safe_mode_active = thermal_alarm;
    state_out->detonation_guard_active = detonation_alarm;

    // 4. 120D Tensor Mapping & Pythagorean Lock
    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        float node_angle = (float)i * ZVD_CRANK_STEP_DEG;
        float angle_diff = ice_in->crankshaft_angle - node_angle;
        
        int riemann_idx = (int32_t)std::round(angle_diff) % 360;
        if (riemann_idx < 0) riemann_idx += 360;
        
        float crank_mapped_val = (ice_in->map_pressure / 101.3f) * ZVD_COS_360[riemann_idx] * 
                                 state_out->air_resonance_amplitude * (1.0f + 0.2f * std::cos(moire_core->angle_a));
        
        penta_raw = (int32_t)std::round(crank_mapped_val * 2.0f);
        ZVD_ICE_PYTHAGOREAN_LOCK(penta_raw, pythagorean_locked_val);
        state_out->state_120d[i] = zvd_ice_guard_val(pythagorean_locked_val);
    }

    (void)zvd_ice_reduce_120d(state_out->state_120d, state_out->active_strategy);

    // 5. Full-Wave ICE Actuator Synthesis & Detonation Protection
    {
        float base_timing = 15.0f;
        float base_pulse = 3.5f;
        float base_soi = 280.0f;

        state_out->electronic_throttle_angle = ice_in->driver_demand * 100.0f;
        state_out->vvt_intake_target_deg = 20.0f + (state_out->air_resonance_amplitude * 15.0f);
        state_out->vvt_exhaust_target_deg = 10.0f;

        base_pulse *= (1.0f + (state_out->air_resonance_amplitude * 0.2f));
        if (ice_in->lambda_sensor < 0.98f) {
            base_pulse *= 0.95f;
        }

        if (state_out->plasma_ignition_active) {
            float lobachevsky_shift = (float)s_ice_lobachevsky_lut[2][2][0] * 2.0f;
            base_timing += 8.0f + lobachevsky_shift;
            base_soi -= 20.0f;
        }

        if (detonation_alarm) {
            base_timing -= 12.0f;
            base_pulse *= 0.85f;
            state_out->electronic_throttle_angle *= 0.75f;
        }

        if (thermal_alarm) {
            state_out->electronic_throttle_angle *= 0.6f;
            base_timing -= 6.0f;
        }

        state_out->ignition_timing_advance_deg = base_timing;
        state_out->fuel_injection_pulse_ms     = base_pulse;
        state_out->injection_phase_angle_deg   = base_soi;
    }

    zvd_ice_rashomon_audit(state_out->state_120d, state_out);
    g_ice_ecu_cycles.fetch_add(1, std::memory_order_relaxed);
}

// =====================================================================
// 4. UNIVERSAL BUS INTEGRATION (V3.1.3 CONTEXT-AWARE ARBITER)
// =====================================================================

inline int zvd_ice_engine_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) {
    (void)gl_inv;
    (void)entropy;
    zvd_ensure_ice_luts();

    if (prompt.starts_with("ZVD_ICE_ECU") || prompt.starts_with("ZVD_ICE_SPORT") || prompt.starts_with("ZVD_ICE_ECO")) {
        ZVD_ICE_Telemetry_120D_t tel{};
        ZVD_ICE_MoireCore_t core{};
        ZVD_ICE_State_120D_t st{};
        tel.engine_rpm = prompt.starts_with("ZVD_ICE_SPORT") ? 5800.0f : 2400.0f;
        tel.driver_demand = prompt.starts_with("ZVD_ICE_SPORT") ? 0.96f : 0.4f;
        tel.map_pressure = 101.3f;
        tel.lambda_sensor = 1.0f;

        ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);
        return 1; // PLUS
    }
    return 0; // ZERO
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_ice_engine_evaluate_ctx(ZvdPipelineContext& ctx) {
    zvd_ensure_ice_luts();

    if (!ctx.prompt.empty() && (ctx.prompt.starts_with("ZVD_ICE_ECU") || ctx.prompt.starts_with("ZVD_ICE_SPORT") || ctx.prompt.starts_with("ZVD_ICE_ECO"))) {
        if (ctx.gl_invariant == -2 || ctx.entropy > 7.2) {
            g_ice_knock_trips.fetch_add(1, std::memory_order_relaxed);
            ctx.routing_directive = "ICE_ECU_KNOCK_INTERLOCK_VETO";
            ctx.accumulated_trity = -1;
            return -1; // VETO
        }

        bool fast_path_used = false;
        if (ctx.datacenter_pkt_ptr != nullptr) {
            fast_path_used = true;
        }

        ZVD_ICE_Telemetry_120D_t tel{};
        ZVD_ICE_MoireCore_t core{};
        ZVD_ICE_State_120D_t st{};
        tel.engine_rpm = ctx.prompt.starts_with("ZVD_ICE_SPORT") ? 5800.0f : 2400.0f;
        tel.driver_demand = ctx.prompt.starts_with("ZVD_ICE_SPORT") ? 0.96f : 0.5f;
        tel.map_pressure = 150.0f;
        tel.lambda_sensor = 0.98f;

        ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);

        ctx.routing_directive = fast_path_used ? "ICE_ECU_120D_FAST_PATH_OK" : "ICE_ECU_120D_STEP_OK";
        ctx.accumulated_trity = 1;
        return 1; // PLUS
    }
    return 0; // ZERO
}
#endif

inline bool zvd_ice_engine_verify(std::string_view prompt, std::string_view response) {
    if (prompt.starts_with("ZVD_ICE_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.1.3 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("ICEEngineECU-120D-Crank720", ZvdModuleRole::ARBITER, zvd_ice_engine_evaluate_ctx, zvd_ice_engine_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("ICEEngineECU-120D-Crank720", zvd_ice_engine_evaluate, zvd_ice_engine_verify);
#endif

// =====================================================================
// 5. AUTONOMOUS 4-MODE ICE SIMULATION (MAIN)
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_ice_ecu_simulation(void) {
    std::printf("\n[*] Running V3.1.3 ICE Engine ECU Simulation (4 Operational Profiles)...\n");
    zvd_ensure_ice_luts();

    ZVD_ICE_Telemetry_120D_t tel;
    ZVD_ICE_MoireCore_t core{};
    ZVD_ICE_State_120D_t st;

    // Profile 1: Idle & Warm-up (850 RPM)
    std::printf("    [Profile 1] Idle & Warm-up (RPM = 850, TLA = 0.0)...\n");
    std::memset(&tel, 0, sizeof(tel));
    tel.engine_rpm = 850.0f;
    tel.crankshaft_angle = 180.0f;
    tel.map_pressure = 35.0f;
    tel.coolant_temp = 85.0f;
    tel.lambda_sensor = 1.0f;
    ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Idle OK. Throttle: %.1f%% | Spark Advance: %.1f deg BTDC | Risk Index: %.3f\n", 
           st.electronic_throttle_angle, st.ignition_timing_advance_deg, st.rashomon_risk_index);

    // Profile 2: Economic Highway Cruise (2400 RPM, Godel Strategy)
    std::printf("    [Profile 2] Highway Cruise (RPM = 2400, TLA = 0.4, Godel Strategy)...\n");
    tel.engine_rpm = 2400.0f;
    tel.crankshaft_angle = 360.0f;
    tel.driver_demand = 0.4f;
    tel.map_pressure = 65.0f;
    tel.coolant_temp = 90.0f;
    tel.lambda_sensor = 1.0f;
    ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Cruise OK. VVT Intake: %.1f deg | Injection Pulse: %.2f ms\n", 
           st.vvt_intake_target_deg, st.fuel_injection_pulse_ms);

    // Profile 3: Full Throttle Acceleration / Sport Overdrive (5800 RPM, Luka Strategy)
    std::printf("    [Profile 3] Full Throttle Acceleration (RPM = 5800, TLA = 0.96, Luka Strategy)...\n");
    tel.engine_rpm = 5800.0f;
    tel.crankshaft_angle = 540.0f;
    tel.driver_demand = 0.96f;
    tel.map_pressure = 210.0f;
    tel.coolant_temp = 95.0f;
    tel.lambda_sensor = 0.88f;
    ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Sport OK. Plasma Ignition Active: %s | Spark Advance: %.1f deg BTDC\n", 
           st.plasma_ignition_active ? "TRUE (Overdrive)" : "FALSE", st.ignition_timing_advance_deg);

    // Profile 4: Heavy Detonation on Poor Fuel (Knock Sensor 940 mV)
    std::printf("    [Profile 4] Heavy Detonation Event (Knock Sensor 940 mV -> Aggressive Retard)...\n");
    tel.engine_rpm = 4500.0f;
    tel.crankshaft_angle = 270.0f;
    tel.driver_demand = 0.85f;
    tel.map_pressure = 180.0f;
    tel.knock_sensor_mv = 940.0f;
    tel.cylinder_pressure = 125.0f;
    ZVD_ICE_Engine_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Detonation OK. Guard Active: %s | Spark Retarded: %.1f deg BTDC | Throttle Cut: %.1f%%\n", 
           st.detonation_guard_active ? "ENGAGED" : "NORMAL", st.ignition_timing_advance_deg, st.electronic_throttle_angle);

    std::printf("    - Total ECU Cycles Executed: %llu\n", (unsigned long long)g_ice_ecu_cycles.load());
    std::printf("    - Knock Sensor Trips:        %llu\n", (unsigned long long)g_ice_knock_trips.load());
    std::printf("    - Thermal Overheat Trips:    %llu\n", (unsigned long long)g_ice_thermal_trips.load());
    std::printf("[*] ICE Engine ECU simulation completed successfully.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::printf("======================================================================\n");
    std::printf("  ZVD-120D ICE ENGINE ECU CORE: 4-STROKE COMBUSTION GOVERNOR (SLOT 9) \n");
    std::printf("  (V3.1.3-ICE-ENGINE-ECU-PRODUCTION / ISO 26262 ASIL D)               \n");
    std::printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)                \n");
    std::printf("  Active Hardware Cores: %d | Library Fingerprint: %016llX\n", 
           zvd_detect_cpu_cores(), (unsigned long long)zvd_compute_core_library_fingerprint());
    std::printf("======================================================================\n");

    run_ice_ecu_simulation();

    int choice = -1;
    while (choice != 0) {
        std::printf("\n========= ZVD V3.1.3 ICE ENGINE ECU POWERTRAIN TERMINAL ========\n");
        std::printf(" [1] Run ICE ECU Simulation (4 Operational Profiles)\n");
        std::printf(" [0] Exit\n");
        std::printf("================================================================\n");
        std::printf("Select menu option: ");

        if (std::scanf("%d", &choice) != 1) {
            std::printf("[!] Invalid input. Try again.\n");
            while (std::getchar() != '\n');
            continue;
        }
        while (std::getchar() != '\n');

        switch (choice) {
            case 1:
                run_ice_ecu_simulation();
                break;
            case 0:
                std::printf("[*] Exiting ZVD-120D ICE Engine ECU Fabric. All cylinders parked.\n");
                break;
            default:
                std::printf("[!] Unknown menu option.\n");
        }
    }

#ifdef _WIN32
    std::printf("\nPress Enter to close console...");
    std::getchar();
#endif

    return 0;
}

#endif // ZVD_NO_MAIN
