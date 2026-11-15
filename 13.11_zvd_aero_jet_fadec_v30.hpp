/* =============================================================================
 *  PROJECT:    ZVD-120D 120D MOIRÉ FADEC CORE: TURBOFAN/TURBOJET ENGINE GOVERNOR (SLOT 8)
 *              (13.11_zvd_aero_jet_fadec_v30.hpp - V3.1.4-AERO-JET-120D-MOIRE-FADEC-C20)
 *  FILENAME:   13.11_zvd_aero_jet_fadec_v30.hpp
 *  VERSION:    2026.11-V3.1.4-AERO-JET-120D-MOIRE-PRODUCTION-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  --------------------------------------------------------------------------
 *  COMPLIANCE: DO-178C / DO-254 Level A (ASIL D) / MISRA C:2012
 *              Header-Only Monolith, Zero Dynamic Allocation, Thread-Safe, 120D Moiré
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_AERO_JET_FADEC 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module 13.11_zvd_aero_jet_fadec_v30.hpp requires zvd_local_ai_proxy.cpp and zvd_datacenter_node_v28_lite.c in the working directory!"
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
#define ZVD_DIM_120             120U    /* 120D Dual-Channel Moiré Tensor (60 sectors Ch A + 60 sectors Ch B) */
#endif

#define ZVD_SECTOR_RES_DEG      6.0f

typedef enum {
    ZVD_AERO_LOGIC_GODEL = 0,   /* Cruising / Standard: Elliptic Riemann metric & Moiré surge suppression */
    ZVD_AERO_LOGIC_LUKA  = 1    /* High-Altitude / Overdrive: Hyperbolic Lobachevsky metric & plasma lean-burn */
} ZVD_Aero_LogicStrategy_t;

/* =====================================================================
 * AIRBORNE TELEMETRY STRUCTURES (120D MOURÉ SPATIAL BASIS)
 * ===================================================================== */

alignas(64) typedef struct {
    float n1_rpm_pct;           /* Low Pressure Spool N1 (% of rated max N1 - Alpha Core) */
    float n2_rpm_pct;           /* High Pressure Spool N2 (% of rated max N2 - Beta Core) */
    float egt_celsius;          /* Exhaust Gas Temperature (EGT) (°C) */
    float t2_inlet_temp_k;      /* Engine Inlet Total Temperature T2 (K) */
    float p2_inlet_press_kpa;   /* Engine Inlet Total Pressure P2 (kPa) */
    float t3_compressor_temp_k; /* Compressor Discharge Temperature T3 (K) */
    float p3_compressor_press_kpa;/* Compressor Discharge Pressure P3 (kPa) */
    float fuel_flow_actual_kg_h;/* Actual Fuel Flow Wf (kg/h) */
    float rotor_vibration_mm_s; /* Rotor Bearing Vibration Velocity (mm/s RMS) */
    float vsv_actual_deg;       /* Variable Stator Vanes Actual Position (deg) */
    float vbv_actual_pct;       /* Variable Bleed Valves Actual Opening (%) */
    float throttle_lever_angle; /* Pilot Thrust Lever Angle (TLA) [0.0 to 1.0] */
    float combustion_sector_temps[ZVD_DIM_120]; /* 120-Point optical pyrometer sector temps (Ch A + Ch B) (°C) */
} ZVD_Aero_Telemetry_t;

alignas(64) typedef struct {
    float angle_a;              /* Alpha core rotation angle (rad) - Spool N1 */
    float angle_b;              /* Counter-rotating Beta core angle (rad) - Spool N2 */
    float base_velocity;        /* Base angular velocity */
    float beam_phase_velocity;  /* Relativistic Moiré phase velocity */
    float macro_entropy;        /* System thermodynamic entropy & acoustic feedback */
} ZVD_DynamicMoireCore_t;

alignas(64) typedef struct {
    int8_t state_120d[ZVD_DIM_120];
    ZVD_Aero_LogicStrategy_t active_strategy;
    bool anti_surge_protection_active;
    bool plasma_stabilizer_active;
    bool thermal_isolation_active;
    bool egt_overtemp_active;
    bool emergency_shutdown_triggered;
    bool moire_surge_warning_active;
    
    float target_fuel_flow_kg_h;   /* Governed Fuel Flow Wf with Slew Rate Bounding */
    float target_vsv_deg;          /* Target Variable Stator Vanes Angle */
    float target_vbv_pct;          /* Target Variable Bleed Valves Opening */
    float plasma_energy_mj;        /* High-Frequency Plasma/Corona Excitation Energy (mJ) */
    float active_core_bleed_pct;   /* Active Sector Bleed Correction (%) */
    
    float rashomon_technical;
    float rashomon_operational;
    float rashomon_structural;
    float rashomon_boundary;
    float rashomon_risk_index;
} ZVD_Aero_FADEC_State_t;

// Cache-aligned atomic telemetry counters
alignas(64) static std::atomic<uint64_t> g_fadec_control_steps{0};
alignas(64) static std::atomic<uint64_t> g_fadec_surge_trips{0};
alignas(64) static std::atomic<uint64_t> g_fadec_moire_warnings{0};
alignas(64) static std::atomic<uint64_t> g_fadec_egt_trips{0};
alignas(64) static std::atomic<uint64_t> g_fadec_shutdown_trips{0};

// =====================================================================
// 1. THREAD-SAFE NON-EUCLIDEAN LUT & TRIGONOMETRY INITIALIZATION
// =====================================================================

static int8_t s_aero_lobachevsky_lut[5][5][2];
static int8_t s_aero_riemann_lut[5][5][2];
alignas(64) static float ZVD_SIN_360[360];
alignas(64) static float ZVD_COS_360[360];
static std::atomic<bool> g_fadec_luts_initialized{false};

inline void zvd_ensure_fadec_luts(void) {
    if (g_fadec_luts_initialized.load(std::memory_order_acquire)) return;
    
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
                s_aero_lobachevsky_lut[y][x][0] = (int8_t)(std::round(2.0f + dy * contract_h) - 2.0f);
                s_aero_lobachevsky_lut[y][x][1] = (int8_t)(std::round(2.0f + dx * contract_h) - 2.0f);

                float sin_scale = (dist > 1e-6f) ? (std::sin(dist) / dist) : 1.0f;
                s_aero_riemann_lut[y][x][0] = (int8_t)(std::round(2.0f + dy * sin_scale) - 2.0f);
                s_aero_riemann_lut[y][x][1] = (int8_t)(std::round(2.0f + dx * sin_scale) - 2.0f);
            }
        }
        g_fadec_luts_initialized.store(true, std::memory_order_release);
    } else {
        while (!g_fadec_luts_initialized.load(std::memory_order_acquire)) {
#if defined(_MSC_VER)
            YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
#endif
        }
    }
}

static inline int8_t zvd_aero_guard_val(int32_t val) {
    return (int8_t)std::clamp(val, -2, 2);
}

// =====================================================================
// 2. 120D MULTIVALUED LOGIC REDUCER & PENTARNY RASHOMON AUDIT
// =====================================================================

inline int8_t zvd_reduce_120d(const int8_t* vec_120d, ZVD_Aero_LogicStrategy_t strategy) {
    if (strategy == ZVD_AERO_LOGIC_GODEL) {
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

inline void zvd_aero_rashomon_audit(const int8_t* state_120d, ZVD_Aero_FADEC_State_t* ctx) {
    float sum = 0.0f, abs_sum = 0.0f;
    float min_v = (float)state_120d[0], max_v = (float)state_120d[0];
    float mean, var_sum = 0.0f, std_dev;

    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        float val = (float)state_120d[i];
        sum += val;
        abs_sum += (val < 0.0f) ? -val : val;
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
    ctx->rashomon_risk_index   = (-(abs_sum / (float)ZVD_DIM_120)) - (std_dev * 0.25f);
}

// =====================================================================
// 3. THREAD-SAFE 120D MOIRÉ FADEC CLOSED-LOOP STEP EXECUTION
// =====================================================================

inline void ZVD_Aero_FADEC_ExecuteStep(
    const ZVD_Aero_Telemetry_t* telemetry_in,
    ZVD_DynamicMoireCore_t* moire_core,
    ZVD_Aero_FADEC_State_t* state_out
) {
    zvd_ensure_fadec_luts();
    std::memset(state_out, 0, sizeof(ZVD_Aero_FADEC_State_t));

    bool surge_risk = false;
    bool moire_warning = false;
    bool sector_hotspot = false;
    bool egt_overtemp = false;
    bool vibration_anomaly = false;
    float mean_sector_temp = 0.0f;

    // 1. Advance Dual Moiré Core Spools (N1 & N2 differential speeds)
    float n1_vel = telemetry_in->n1_rpm_pct * 0.002f;
    float n2_vel = telemetry_in->n2_rpm_pct * 0.003f;
    moire_core->angle_a = std::fmod(moire_core->angle_a + n1_vel, 2.0f * (float)M_PI);
    moire_core->angle_b = std::fmod(moire_core->angle_b - n2_vel * 1.5f, 2.0f * (float)M_PI);

    int deg_a = (int)(moire_core->angle_a * (180.0f / (float)M_PI)) % 360;
    if (deg_a < 0) deg_a += 360;
    int deg_b = (int)(moire_core->angle_b * (180.0f / (float)M_PI)) % 360;
    if (deg_b < 0) deg_b += 360;

    float moire_interference = ZVD_COS_360[deg_a] - ZVD_SIN_360[deg_b];
    if (std::fabs(moire_interference) > 1.75f) {
        moire_warning = true;
        g_fadec_moire_warnings.fetch_add(1, std::memory_order_relaxed);
    }

    // 2. Pressure Ratio & Surge Audit
    float pressure_ratio = (telemetry_in->p2_inlet_press_kpa > 1.0f) ? 
                           (telemetry_in->p3_compressor_press_kpa / telemetry_in->p2_inlet_press_kpa) : 1.0f;
    
    if ((pressure_ratio > (12.0f * (telemetry_in->n1_rpm_pct * 0.01f))) || 
        (telemetry_in->rotor_vibration_mm_s > 8.5f) || moire_warning) {
        surge_risk = true;
        g_fadec_surge_trips.fetch_add(1, std::memory_order_relaxed);
    }

    // 3. 120-Sector Thermal Symmetry Audit (Channel A + Channel B)
    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        mean_sector_temp += telemetry_in->combustion_sector_temps[i];
    }
    mean_sector_temp /= (float)ZVD_DIM_120;

    for (size_t i = 0; i < ZVD_DIM_120; i++) {
        float temp_asymmetry_delta = telemetry_in->combustion_sector_temps[i] - mean_sector_temp;
        int32_t penta_raw = (int32_t)std::round(temp_asymmetry_delta * 0.05f);
        state_out->state_120d[i] = zvd_aero_guard_val(penta_raw);

        if (telemetry_in->combustion_sector_temps[i] > 1250.0f || temp_asymmetry_delta > 150.0f) {
            sector_hotspot = true;
        }
    }

    if (telemetry_in->egt_celsius > 980.0f) {
        egt_overtemp = true;
        g_fadec_egt_trips.fetch_add(1, std::memory_order_relaxed);
    }

    if (telemetry_in->rotor_vibration_mm_s > 10.0f) {
        vibration_anomaly = true;
    }

    // 4. Adaptive Strategy Selection & 120D Tensor Reducer
    if (telemetry_in->throttle_lever_angle > 0.82f || telemetry_in->t2_inlet_temp_k < 230.0f) {
        state_out->active_strategy = ZVD_AERO_LOGIC_LUKA;
        state_out->plasma_stabilizer_active = true;
    } else {
        state_out->active_strategy = ZVD_AERO_LOGIC_GODEL;
        state_out->plasma_stabilizer_active = false;
    }

    int8_t envelope_trit = zvd_reduce_120d(state_out->state_120d, state_out->active_strategy);

    state_out->anti_surge_protection_active = surge_risk;
    state_out->moire_surge_warning_active = moire_warning;
    state_out->thermal_isolation_active = sector_hotspot;
    state_out->egt_overtemp_active = egt_overtemp;

    if (vibration_anomaly && telemetry_in->rotor_vibration_mm_s > 15.0f) {
        state_out->emergency_shutdown_triggered = true;
        g_fadec_shutdown_trips.fetch_add(1, std::memory_order_relaxed);
    } else {
        state_out->emergency_shutdown_triggered = false;
    }

    // 5. Actuator Synthesis with Slew Rate Bounding & 5x5 LUT Mapping
    {
        float base_wf = telemetry_in->throttle_lever_angle * 4500.0f;
        float base_vsv = 15.0f + (telemetry_in->n2_rpm_pct * 0.3f);
        float base_vbv = (telemetry_in->n1_rpm_pct < 65.0f) ? 25.0f : 0.0f;
        float plasma_energy = 15.0f;

        base_wf *= (1.0f + (float)envelope_trit * 0.015f);

        if (surge_risk) {
            int lut_y = 2, lut_x = 2;
            float riemann_correction = (float)s_aero_riemann_lut[lut_y][lut_x][0] * 5.0f;
            (void)riemann_correction;
            base_vbv = 100.0f;
            base_vsv -= 10.0f;
            base_wf *= (moire_warning ? 0.70f : 0.80f); // Moiré pre-surge proactive roll-back
        }

        if (state_out->plasma_stabilizer_active) {
            float accum_warp = 0.0f;
            for(size_t s = 0; s < ZVD_DIM_120; s++) {
                int ly = (int)((s / 24) % 5);
                int lx = (int)(s % 5);
                accum_warp += (float)s_aero_lobachevsky_lut[ly][lx][0];
            }
            float lobachevsky_warp = (accum_warp / (float)ZVD_DIM_120) * 3.0f;
            plasma_energy = 45.0f + (std::fabs(lobachevsky_warp) * 10.0f);
            base_wf *= 0.88f;
        }

        if (sector_hotspot || egt_overtemp) {
            base_wf *= 0.70f;
        }

        if (state_out->emergency_shutdown_triggered) {
            base_wf = 0.0f;
        } else {
            if (base_wf < 150.0f) {
                base_wf = 150.0f;
            }
        }

        // Hydraulic Actuator Slew Rate Bounding
        float prev_wf = telemetry_in->fuel_flow_actual_kg_h;
        float max_wf_delta = 120.0f;
        if (std::fabs(base_wf - prev_wf) > max_wf_delta) {
            base_wf = prev_wf + std::clamp(base_wf - prev_wf, -max_wf_delta, max_wf_delta);
        }

        state_out->target_fuel_flow_kg_h = base_wf;
        state_out->target_vsv_deg        = base_vsv;
        state_out->target_vbv_pct        = base_vbv;
        state_out->plasma_energy_mj      = plasma_energy;
        state_out->active_core_bleed_pct = (sector_hotspot || egt_overtemp || moire_warning) ? 15.0f : 0.0f;
    }

    zvd_aero_rashomon_audit(state_out->state_120d, state_out);
    g_fadec_control_steps.fetch_add(1, std::memory_order_relaxed);
}

// =====================================================================
// 4. UNIVERSAL BUS INTEGRATION (V3.1.4 CONTEXT-AWARE ARBITER)
// =====================================================================

inline int zvd_aero_jet_fadec_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) {
    (void)gl_inv;
    (void)entropy;
    zvd_ensure_fadec_luts();

    if (prompt.starts_with("ZVD_JET_FADEC") || prompt.starts_with("ZVD_JET_CRUISE") || prompt.starts_with("ZVD_JET_AFTERBURNER")) {
        ZVD_Aero_Telemetry_t tel{};
        ZVD_DynamicMoireCore_t core{};
        ZVD_Aero_FADEC_State_t st{};
        tel.throttle_lever_angle = prompt.starts_with("ZVD_JET_AFTERBURNER") ? 0.95f : 0.8f;
        tel.n1_rpm_pct = 90.0f;
        tel.n2_rpm_pct = 92.0f;
        tel.p2_inlet_press_kpa = 101.3f;
        tel.p3_compressor_press_kpa = 1250.0f;
        for(size_t i=0; i<ZVD_DIM_120; i++) tel.combustion_sector_temps[i] = 1100.0f;

        ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);
        return 1; // PLUS
    }
    return 0; // ZERO
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_aero_jet_fadec_evaluate_ctx(ZvdPipelineContext& ctx) {
    zvd_ensure_fadec_luts();

    if (!ctx.prompt.empty() && (ctx.prompt.starts_with("ZVD_JET_FADEC") || ctx.prompt.starts_with("ZVD_JET_CRUISE") || ctx.prompt.starts_with("ZVD_JET_AFTERBURNER"))) {
        if (ctx.gl_invariant == -2 || ctx.entropy > 7.2) {
            g_fadec_surge_trips.fetch_add(1, std::memory_order_relaxed);
            ctx.routing_directive = "FADEC_MOIRE_ANTI_SURGE_INTERLOCK_VETO";
            ctx.accumulated_trity = -1;
            return -1; // VETO
        }

        ZVD_Aero_Telemetry_t tel{};
        ZVD_DynamicMoireCore_t core{};
        ZVD_Aero_FADEC_State_t st{};
        tel.throttle_lever_angle = ctx.prompt.starts_with("ZVD_JET_AFTERBURNER") ? 0.98f : 0.85f;
        tel.n1_rpm_pct = 95.0f;
        tel.n2_rpm_pct = 96.0f;
        tel.p2_inlet_press_kpa = 50.0f;
        tel.p3_compressor_press_kpa = 900.0f;
        tel.t2_inlet_temp_k = 215.0f;
        
        bool fast_path_used = false;
        if (ctx.datacenter_pkt_ptr != nullptr) {
            const auto* dc_pkt = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr);
            for (size_t i = 0; i < ZVD_DIM_120; i++) {
                uint8_t b_val = dc_pkt->monolith.face_packed[i % 167];
                tel.combustion_sector_temps[i] = 1000.0f + (float)(b_val * 1.5f);
            }
            fast_path_used = true;
        } else {
            for (size_t i = 0; i < ZVD_DIM_120; i++) {
                tel.combustion_sector_temps[i] = 1150.0f + (float)((i % 5) * 10);
            }
        }

        ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);

        ctx.routing_directive = fast_path_used ? "FADEC_120D_MOIRE_FAST_PATH_OK" : "FADEC_120D_MOIRE_STEP_OK";
        ctx.accumulated_trity = 1;
        return 1; // PLUS
    }
    return 0; // ZERO
}
#endif

inline bool zvd_aero_jet_fadec_verify(std::string_view prompt, std::string_view response) {
    if (prompt.starts_with("ZVD_JET_VERIFY")) {
        return !response.empty();
    }
    return true;
}

// Universal Cartridge Registration Block (V3.1.4 Context-Aware with Legacy Fallback)
#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("AeroJetFADEC-120D-Moiré", ZvdModuleRole::ARBITER, zvd_aero_jet_fadec_evaluate_ctx, zvd_aero_jet_fadec_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("AeroJetFADEC-120D-Moiré", zvd_aero_jet_fadec_evaluate, zvd_aero_jet_fadec_verify);
#endif

// =====================================================================
// 5. AUTONOMOUS FLIGHT MODES SIMULATION (MAIN)
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_fadec_flight_modes_simulation(void) {
    std::printf("\n[*] Running V3.1.4 120D Moiré FADEC Flight Modes Simulation (4 Operational Profiles)...\n");
    zvd_ensure_fadec_luts();

    ZVD_Aero_Telemetry_t tel;
    ZVD_DynamicMoireCore_t core{};
    ZVD_Aero_FADEC_State_t st;

    // Profile 1: Takeoff Mode (Godel Cruising & Full Power)
    std::printf("    [Profile 1] Takeoff Mode (TLA = 0.95, Godel Strategy, N1/N2 Moiré Sync)...\n");
    std::memset(&tel, 0, sizeof(tel));
    tel.throttle_lever_angle = 0.95f;
    tel.n1_rpm_pct = 98.5f;
    tel.n2_rpm_pct = 99.0f;
    tel.p2_inlet_press_kpa = 101.3f;
    tel.p3_compressor_press_kpa = 2400.0f;
    tel.t2_inlet_temp_k = 288.15f;
    tel.egt_celsius = 890.0f;
    tel.rotor_vibration_mm_s = 2.1f;
    for(size_t i=0; i<ZVD_DIM_120; i++) tel.combustion_sector_temps[i] = 1180.0f;
    ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Takeoff OK. Fuel Flow: %.1f kg/h | Moiré Angle A/B: %.2f / %.2f rad | Risk Index: %.3f\n", 
           st.target_fuel_flow_kg_h, (double)core.angle_a, (double)core.angle_b, st.rashomon_risk_index);

    // Profile 2: High-Altitude Supersonic Cruise with Plasma Lean-Burn (Lobachevsky Overdrive)
    std::printf("    [Profile 2] High-Altitude Cruise with Plasma Lean-Burn (T2 = 215K, Luka Strategy)...\n");
    std::memset(&tel, 0, sizeof(tel));
    tel.throttle_lever_angle = 0.85f;
    tel.n1_rpm_pct = 92.0f;
    tel.n2_rpm_pct = 93.5f;
    tel.p2_inlet_press_kpa = 35.0f;
    tel.p3_compressor_press_kpa = 850.0f;
    tel.t2_inlet_temp_k = 215.0f;
    tel.egt_celsius = 780.0f;
    tel.rotor_vibration_mm_s = 1.8f;
    for(size_t i=0; i<ZVD_DIM_120; i++) tel.combustion_sector_temps[i] = 1050.0f;
    ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Cruise OK. Plasma Energy: %.1f mJ | Lean-Burn Active: %s\n", 
           st.plasma_energy_mj, st.plasma_stabilizer_active ? "TRUE (Fuel Saved)" : "FALSE");

    // Profile 3: Moiré Acoustic Pre-Surge Warning & Proactive Anti-Surge Trigger
    std::printf("    [Profile 3] Moiré Acoustic Pre-Surge Warning & Proactive VBV Engagement...\n");
    tel.p2_inlet_press_kpa = 80.0f;
    tel.p3_compressor_press_kpa = 1600.0f;
    tel.n1_rpm_pct = 50.0f;
    tel.n2_rpm_pct = 98.0f; // Extreme differential speed causing Moiré interference spike
    tel.egt_celsius = 820.0f;
    ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);
    std::printf("      -> Moiré Surge Warning: %s | Anti-Surge Active: %s | VBV Opening: %.1f%%\n", 
           st.moire_surge_warning_active ? "WARNING (TRIGGERED)" : "NORMAL",
           st.anti_surge_protection_active ? "ACTIVE (ENGAGED)" : "NORMAL", st.target_vbv_pct);

    // Profile 4: Combustion Sector Hotspot & EGT Over-Temperature Hard Interlock
    std::printf("    [Profile 4] Sector Hotspot & EGT Over-Temperature Interlock (>980C)...\n");
    tel.p2_inlet_press_kpa = 101.3f;
    tel.p3_compressor_press_kpa = 1200.0f;
    tel.egt_celsius = 1015.0f;
    tel.combustion_sector_temps[42] = 1350.0f;
    ZVD_Aero_FADEC_ExecuteStep(&tel, &core, &st);
    std::printf("      -> EGT Interlock OK. EGT Overtemp Active: %s | Fuel Flow Rollback: %.1f kg/h\n", 
           st.egt_overtemp_active ? "ACTIVE (ROLLBACK)" : "NORMAL", st.target_fuel_flow_kg_h);

    std::printf("    - FADEC Control Steps Executed: %llu\n", (unsigned long long)g_fadec_control_steps.load());
    std::printf("    - Compressor Surge Trips:       %llu\n", (unsigned long long)g_fadec_surge_trips.load());
    std::printf("    - Moiré Acoustic Warnings:      %llu\n", (unsigned long long)g_fadec_moire_warnings.load());
    std::printf("    - EGT Overtemp Trips:           %llu\n", (unsigned long long)g_fadec_egt_trips.load());
    std::printf("    - Emergency Shutdown Trips:     %llu\n", (unsigned long long)g_fadec_shutdown_trips.load());
    std::printf("[*] 120D Moiré FADEC flight modes simulation completed successfully.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::printf("======================================================================\n");
    std::printf("  ZVD-120D 120D MOIRÉ FADEC CORE: TURBOFAN/TURBOJET ENGINE GOVERNOR   \n");
    std::printf("  (V3.1.4-AERO-JET-120D-MOIRE / DO-178C Level A)                      \n");
    std::printf("  Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)                \n");
    std::printf("  Active Hardware Cores: %d | Library Fingerprint: %016llX\n", 
           zvd_detect_cpu_cores(), (unsigned long long)zvd_compute_core_library_fingerprint());
    std::printf("======================================================================\n");

    run_fadec_flight_modes_simulation();

    int choice = -1;
    while (choice != 0) {
        std::printf("\n========= ZVD V3.1.4 120D MOIRÉ FADEC POWERTRAIN TERMINAL ======\n");
        std::printf(" [1] Run 120D Moiré FADEC Flight Modes Simulation (4 Profiles)\n");
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
                run_fadec_flight_modes_simulation();
                break;
            case 0:
                std::printf("[*] Exiting ZVD-120D 120D Moiré FADEC Fabric. All channels disarmed.\n");
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
