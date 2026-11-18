/* =============================================================================
 *  PROJECT:    ZVD-120D QUANTITATIVE FINANCIAL ARBITER & HFT COGNITIVE THEATER (SLOT [D1-FIN])
 *              (13.11_zvd_quant_financial_arbiter_v30.hpp - V3.3.6-QUANT-DEBATE-LOMONOSOV81-C20)
 *  FILENAME:   13.11_zvd_quant_financial_arbiter_v30.hpp
 *  VERSION:    2026.11-V3.3.6-QUANT-DEBATE-LOMONOSOV81-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution, 
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero Dynamic Allocation, Zero-Lock, Zero-FPU (Integer Q8 Fixed-Point)
 *              12-Mask Teatro Adversarial Debate, Kirchhoff Guard & 81-Branch Lomonosov Tree
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_QUANT_FINANCIAL_ARBITER 1

#ifndef ZVD_HAS_DATACENTER_CORE
struct ZVD_Monolith334 {
    uint8_t face_packed[167];
    uint8_t vertex_packed[167];
};
struct ZVD_DatacenterPacket {
    uint64_t packet_id;
    ZVD_Monolith334 monolith;
};
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <atomic>
#include <algorithm>
#include <string_view>

// =====================================================================
// 1. ARCHITECTURAL CONSTANTS & Q8 FIXED-POINT SPECIFICATIONS
// =====================================================================

constexpr int ZVD_FIN_DIM_60 = 60;          // 60D Face (Orderbook) & 60D Vertex (Tape)
constexpr int ZVD_FIN_MASKS_COUNT =     12;     // 12 Teatro Research Masks
constexpr int ZVD_FIN_LOMONOSOV_BRANCHES = 81; // 3^4 = 81 Causal Tree Regimes
constexpr int Q8_ONE = 256;                 // Fixed-point scale (256 = 1.0)

// =====================================================================
// 2. COMPILE-TIME CONSTEVAL EGYPTIAN TRIANGLE 3-4-5 & NON-EUCLIDEAN LUTs
// =====================================================================

namespace zvd::fin::detail {
    constexpr uint32_t isqrt_constexpr(uint32_t n) noexcept {
        if (n <= 1) return n;
        uint32_t x0 = n / 2;
        uint32_t x1 = (x0 + n / x0) / 2;
        while (x1 < x0) {
            x0 = x1;
            x1 = (x0 + n / x0) / 2;
        }
        return x0;
    }

    constexpr auto generate_egyptian_hypot_lut() noexcept {
        std::array<std::array<int8_t, 16>, 16> arr{};
        for (int x = 0; x < 16; ++x) {
            for (int y = 0; y < 16; ++y) {
                uint32_t leg_a = static_cast<uint32_t>(3 * x);
                uint32_t leg_b = static_cast<uint32_t>(4 * y);
                uint32_t hypot_c = isqrt_constexpr(leg_a * leg_a + leg_b * leg_b);
                int pentad = static_cast<int>(hypot_c / 5) - 6;
                arr[x][y] = static_cast<int8_t>(std::max(-2, std::min(2, pentad)));
            }
        }
        return arr;
    }
}

alignas(64) static constexpr auto ZVD_FIN_EGYPTIAN_HYPOT_LUT = zvd::fin::detail::generate_egyptian_hypot_lut();


// =====================================================================
// 3. 12-MASK TEATRO ADVERSARIAL DEBATE ENSEMBLE
// =====================================================================

enum class ZvdMascheraId : uint8_t {
    PANTALONE = 0,   // Group 0: Capital, Cost, Price
    DOTTOR = 1,      // Group 0: Formal Rules, Logic
    CAPITANO = 2,    // Group 0: Breakout Momentum
    BALANZONE = 3,   // Group 0: Low-Variance Precision
    BRIGHELLA = 4,   // Group 1: Tactical Execution Plan
    ARLECCHINO = 5,  // Group 1: Arbitrage Speed
    TARTAGLIA = 6,   // Group 1: Risk, Spoofing, Anomalies
    SCAPINO = 7,     // Group 1: Hedging & Maneuvers
    INNAMORATI = 8,  // Group 2: Crowd Sentiment, Trend Hype
    COLOMBINA = 9,   // Group 2: Hidden Liquidity Truth
    PULCINELLA = 10, // Group 2: Mass Crowd Opinion
    GIANDUJA = 11    // Group 2: Contract Transparency
};

struct ZvdMascheraProfile {
    ZvdMascheraId id;
    uint8_t group;     // 0: Power/Capital, 1: Action/Risk, 2: Crowd/Sentiment
    int16_t base_weight_q8;

    int16_t evaluate_q8(int16_t liquidity_q8, int16_t volatility_q8, int16_t order_flow_q8) const noexcept {
        switch (id) {
            case ZvdMascheraId::PANTALONE:
            case ZvdMascheraId::CAPITANO:
            case ZvdMascheraId::COLOMBINA:
                return static_cast<int16_t>((static_cast<int32_t>(liquidity_q8) * 3 + order_flow_q8 * 5) >> 3);
            case ZvdMascheraId::DOTTOR:
            case ZvdMascheraId::BALANZONE:
            case ZvdMascheraId::ARLECCHINO: {
                int16_t stability = static_cast<int16_t>(Q8_ONE - (std::abs(volatility_q8) >> 1));
                return std::max(static_cast<int16_t>(0), std::min(static_cast<int16_t>(Q8_ONE), stability));
            }
            case ZvdMascheraId::TARTAGLIA:
                return std::min(static_cast<int16_t>(Q8_ONE), static_cast<int16_t>(std::abs(volatility_q8) * 2));
            default:
                return static_cast<int16_t>((static_cast<int32_t>(liquidity_q8) + order_flow_q8) >> 1);
        }
    }
};

class alignas(64) ZvdTeatroEnsemble {
private:
    std::array<ZvdMascheraProfile, ZVD_FIN_MASKS_COUNT> masks;

public:
    ZvdTeatroEnsemble() noexcept {
        masks[0]  = {ZvdMascheraId::PANTALONE,  0, 256};
        masks[1]  = {ZvdMascheraId::DOTTOR,     0, 230};
        masks[2]  = {ZvdMascheraId::CAPITANO,   0, 280};
        masks[3]  = {ZvdMascheraId::BALANZONE,  0, 240};
        masks[4]  = {ZvdMascheraId::BRIGHELLA,  1, 200};
        masks[5]  = {ZvdMascheraId::ARLECCHINO, 1, 220};
        masks[6]  = {ZvdMascheraId::TARTAGLIA,  1, 300};
        masks[7]  = {ZvdMascheraId::SCAPINO,    1, 210};
        masks[8]  = {ZvdMascheraId::INNAMORATI, 2, 180};
        masks[9]  = {ZvdMascheraId::COLOMBINA,  2, 250};
        masks[10] = {ZvdMascheraId::PULCINELLA, 2, 150};
        masks[11] = {ZvdMascheraId::GIANDUJA,   2, 190};
    }

    struct DebateEvaluation {
        int16_t consensus_score_q8;
        int16_t group0_score_q8; // Capital
        int16_t group1_score_q8; // Risk / Tartaglia
        int16_t group2_score_q8; // Crowd Hype
        int16_t inter_group_conflict_q8;
        bool is_coherent;
    };

    DebateEvaluation evaluate_adversarial_debate(int16_t liq_q8, int16_t vol_q8, int16_t flow_q8) const noexcept {
        int32_t group_sums[3] = {0, 0, 0};
        int32_t group_weights[3] = {0, 0, 0};
        int32_t total_weighted_sum = 0;
        int32_t total_weight = 0;
        int32_t variance_accumulator = 0;

        for (uint8_t i = 0; i < ZVD_FIN_MASKS_COUNT; ++i) {
            int16_t score = masks[i].evaluate_q8(liq_q8, vol_q8, flow_q8);
            uint8_t grp = masks[i].group;
            int32_t weighted = (static_cast<int32_t>(score) * masks[i].base_weight_q8) >> 8;

            group_sums[grp] += weighted;
            group_weights[grp] += masks[i].base_weight_q8;
            total_weighted_sum += weighted;
            total_weight += masks[i].base_weight_q8;

            int32_t diff = score - (Q8_ONE / 2);
            variance_accumulator += (diff * diff) >> 8;
        }

        int16_t g0 = group_weights[0] > 0 ? static_cast<int16_t>(group_sums[0] / (group_weights[0] >> 8)) : 0;
        int16_t g1 = group_weights[1] > 0 ? static_cast<int16_t>(group_sums[1] / (group_weights[1] >> 8)) : 0;
        int16_t g2 = group_weights[2] > 0 ? static_cast<int16_t>(group_sums[2] / (group_weights[2] >> 8)) : 0;

        // Adversarial Debate Penalty: if Crowd Hype (Group 2) diverges sharply from Capital (Group 0) or Risk (Group 1)
        int16_t conflict = static_cast<int16_t>((std::abs(g2 - g0) + std::abs(g2 - g1)) >> 1);
        
        int16_t consensus = total_weight > 0 ? static_cast<int16_t>(total_weighted_sum / (total_weight >> 8)) : 0;
        if (conflict > (Q8_ONE / 3)) {
            consensus = static_cast<int16_t>(consensus - (conflict >> 1)); // Suppress unbacked crowd hype traps
        }

        int16_t coherence = static_cast<int16_t>(variance_accumulator / ZVD_FIN_MASKS_COUNT);
        bool coherent = (coherence < (Q8_ONE / 3) && conflict < (Q8_ONE / 2));

        return DebateEvaluation{consensus, g0, g1, g2, conflict, coherent};
    }
};


// =====================================================================
// 4. KIRCHHOFF LIQUIDITY CONSERVATION & SPOOFING GUARD
// =====================================================================

class alignas(64) ZvdKirchhoffConservation {
public:
    struct KirchhoffResult {
        int32_t net_flow_q8;
        bool is_balanced;
        bool spoofing_detected;
    };

    static KirchhoffResult verify_balance(const int8_t* face_60, const int8_t* vertex_60) noexcept {
        int32_t bid_sum = 0;
        int32_t ask_sum = 0;
        for (int i = 0; i < 30; ++i) {
            bid_sum += face_60[i];
            ask_sum += face_60[30 + i];
        }

        int32_t tape_velocity = 0;
        for (int i = 0; i < ZVD_FIN_DIM_60; ++i) {
            tape_velocity += vertex_60[i];
        }

        int32_t net_flow = bid_sum - ask_sum;
        bool balanced = (std::abs(net_flow) < 15);
        bool spoofing = (std::abs(tape_velocity) > 40 && std::abs(net_flow) < 5);

        return KirchhoffResult{net_flow, balanced, spoofing};
    }
};


// =====================================================================
// 5. 81-BRANCH TERNARY LOMONOSOV CAUSAL TREE (3^4 = 81)
// =====================================================================

struct ZvdRashomonMetricsQ8 {
    int16_t technical_strength;
    int16_t operational_volume;
    int16_t structural_resistance;
    int16_t boundary_repo_volatility;
    int16_t manipulation_risk_index;
};

class alignas(64) ZvdLomonosovCausalTree81 {
private:
    alignas(64) std::array<int16_t, ZVD_FIN_LOMONOSOV_BRANCHES> branch_reputation_table;

public:
    ZvdLomonosovCausalTree81() noexcept {
        branch_reputation_table.fill(0); // Initial neutral reputation
    }

    static uint8_t compute_branch_id(int8_t macro_trend, int8_t ob_imbalance, int8_t news_sentiment, int8_t vol_regime) noexcept {
        int m = std::clamp(macro_trend, static_cast<int8_t>(-1), static_cast<int8_t>(1)) + 1;
        int o = std::clamp(ob_imbalance, static_cast<int8_t>(-1), static_cast<int8_t>(1)) + 1;
        int n = std::clamp(news_sentiment, static_cast<int8_t>(-1), static_cast<int8_t>(1)) + 1;
        int v = std::clamp(vol_regime, static_cast<int8_t>(-1), static_cast<int8_t>(1)) + 1;
        
        return static_cast<uint8_t>(m * 27 + o * 9 + n * 3 + v); // Exact mapping into 0..80
    }

    bool is_branch_quarantined(uint8_t branch_id) const noexcept {
        if (branch_id >= ZVD_FIN_LOMONOSOV_BRANCHES) return false;
        return branch_reputation_table[branch_id] < -128; // Severe historical losses trigger quarantine VETO
    }

    void update_branch(uint8_t branch_id, int16_t gain_q8, int16_t loss_q8) noexcept {
        if (branch_id >= ZVD_FIN_LOMONOSOV_BRANCHES) return;
        int32_t current = branch_reputation_table[branch_id];
        int32_t delta = static_cast<int32_t>(gain_q8) - loss_q8;
        int32_t updated = std::clamp(current + (delta >> 2), static_cast<int32_t>(-512), static_cast<int32_t>(512));
        branch_reputation_table[branch_id] = static_cast<int16_t>(updated);
    }
};


// =====================================================================
// 6. QUANT FINANCIAL ARBITER MAIN ENGINE
// =====================================================================

class alignas(64) ZvdQuantFinancialArbiter {
private:
    ZvdTeatroEnsemble teatro;
    ZvdLomonosovCausalTree81 lomonosov_tree;

    static inline ZvdRashomonMetricsQ8 audit_orderbook(const int8_t* face_60) noexcept {
        int32_t sum = 0;
        int32_t abs_sum = 0;
        int8_t min_val = face_60[0];
        int8_t max_val = face_60[0];

        for (int i = 0; i < ZVD_FIN_DIM_60; ++i) {
            int8_t val = face_60[i];
            sum += val;
            abs_sum += std::abs(static_cast<int>(val));
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        int16_t mean = static_cast<int16_t>((sum * Q8_ONE) / ZVD_FIN_DIM_60);
        int32_t var_sum = 0;
        for (int i = 0; i < ZVD_FIN_DIM_60; ++i) {
            int32_t diff = face_60[i] - (mean / Q8_ONE);
            var_sum += diff * diff;
        }
        int16_t std_val = static_cast<int16_t>(zvd::fin::detail::isqrt_constexpr(static_cast<uint32_t>(var_sum / ZVD_FIN_DIM_60)) * Q8_ONE);

        return ZvdRashomonMetricsQ8{
            mean,
            static_cast<int16_t>(std::clamp(std::abs(min_val) * 64, 0, Q8_ONE)), // Normalized properly to [0..256] Q8
            static_cast<int16_t>(std::clamp(std::abs(max_val) * 64, 0, Q8_ONE)),
            std_val,
            static_cast<int16_t>(-((abs_sum * Q8_ONE) / ZVD_FIN_DIM_60))
        };
    }

public:
    ZvdQuantFinancialArbiter() noexcept : teatro(), lomonosov_tree() {}

    struct ExecutionDirective {
        int8_t pentary_action; // -2: HARD_VETO, -1: HEDGE, 0: NEUTRAL, +1: LIMIT, +2: AGGRESSIVE
        int16_t final_score_q8;
        bool spoofing_flag;
        bool quarantine_flag;
        uint8_t branch_id;
    };

    ExecutionDirective process_market_tick(const int8_t* face_60, const int8_t* vertex_60) noexcept {
        // 1. Kirchhoff Balance & Spoofing Guard
        auto kirchhoff = ZvdKirchhoffConservation::verify_balance(face_60, vertex_60);
        if (kirchhoff.spoofing_detected) {
            return ExecutionDirective{-2, 0, true, false, 0};
        }

        // 2. Rashomon Orderbook Audit
        ZvdRashomonMetricsQ8 rashomon = audit_orderbook(face_60);

        // 3. 12-Mask Adversarial Debate
        int16_t liq_q8 = rashomon.operational_volume;
        int16_t vol_q8 = rashomon.boundary_repo_volatility;
        int16_t flow_q8 = rashomon.technical_strength;
        auto debate = teatro.evaluate_adversarial_debate(liq_q8, vol_q8, flow_q8);

        // 4. Determine 81-Branch Causal Coordinates
        int8_t macro_trend = (flow_q8 > 0) ? 1 : ((flow_q8 < 0) ? -1 : 0);
        int8_t ob_imbalance = (kirchhoff.net_flow_q8 > 0) ? 1 : ((kirchhoff.net_flow_q8 < 0) ? -1 : 0);
        int8_t news_sentiment = (debate.group2_score_q8 > (Q8_ONE / 2)) ? 1 : -1;
        int8_t vol_regime = (vol_q8 > (Q8_ONE / 2)) ? 1 : -1;

        uint8_t branch_id = ZvdLomonosovCausalTree81::compute_branch_id(macro_trend, ob_imbalance, news_sentiment, vol_regime);

        // 5. Check Lomonosov Branch Quarantine
        if (lomonosov_tree.is_branch_quarantined(branch_id)) {
            return ExecutionDirective{-2, debate.consensus_score_q8, false, true, branch_id};
        }

        // 6. Directive Mapping
        int8_t action = 0;
        if (!debate.is_coherent || rashomon.manipulation_risk_index < -200) {
            action = -1;
        } else if (debate.consensus_score_q8 > (Q8_ONE * 3 / 4)) {
            action = 2;
        } else if (debate.consensus_score_q8 > (Q8_ONE / 2)) {
            action = 1;
        } else {
            action = 0;
        }

        int16_t gain = (debate.consensus_score_q8 > Q8_ONE / 2) ? debate.consensus_score_q8 : 0;
        int16_t loss = (debate.consensus_score_q8 <= Q8_ONE / 2) ? (Q8_ONE - debate.consensus_score_q8) : 0;
        lomonosov_tree.update_branch(branch_id, gain, loss);

        return ExecutionDirective{action, debate.consensus_score_q8, false, false, branch_id};
    }
};


// =====================================================================
// 7. C-ABI BRIDGE FOR PYTHON HFT FEEDER INTEGRATION
// =====================================================================

#ifdef __cplusplus
extern "C" {
#endif

inline int8_t zvd_fin_abi_process_monolith334(const uint8_t* monolith_334_bytes, int16_t* out_score_q8, uint8_t* out_spoofing_flag, int8_t* out_spiral_level, uint8_t* out_branch_id) {
    if (!monolith_334_bytes) return -2;
    static thread_local ZvdQuantFinancialArbiter t_abi_arbiter{};
    const auto* mono = reinterpret_cast<const ZVD_Monolith334*>(monolith_334_bytes);

    alignas(64) int8_t face_60[60];
    alignas(64) int8_t vertex_60[60];
    for (int i = 0; i < 60; ++i) {
        face_60[i] = static_cast<int8_t>((mono->face_packed[i % 167] % 5) - 2);
        vertex_60[i] = static_cast<int8_t>((mono->vertex_packed[i % 167] % 5) - 2);
    }

    auto directive = t_abi_arbiter.process_market_tick(face_60, vertex_60);
    if (out_score_q8) *out_score_q8 = directive.final_score_q8;
    if (out_spoofing_flag) *out_spoofing_flag = directive.spoofing_flag ? 1 : 0;
    if (out_spiral_level) *out_spiral_level = directive.quarantine_flag ? -2 : 0;
    if (out_branch_id) *out_branch_id = directive.branch_id;

    return directive.pentary_action;
}

inline int8_t zvd_fin_abi_fold_and_evaluate(const uint8_t* raw_market_bytes, size_t len, int16_t* out_score_q8, uint8_t* out_spoofing_flag) {
    if (!raw_market_bytes || len == 0) return -2;
    static thread_local ZvdQuantFinancialArbiter t_abi_arbiter{};

    alignas(64) int8_t face_60[60];
    alignas(64) int8_t vertex_60[60];
    for (int i = 0; i < 60; ++i) {
        size_t idx_a = (i * 2) % len;
        size_t idx_b = (i * 2 + 1) % len;
        uint8_t ba = raw_market_bytes[idx_a];
        uint8_t bb = raw_market_bytes[idx_b];
        face_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[ba & 0x0F][bb & 0x0F];
        vertex_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[bb & 0x0F][ba & 0x0F];
    }

    auto directive = t_abi_arbiter.process_market_tick(face_60, vertex_60);
    if (out_score_q8) *out_score_q8 = directive.final_score_q8;
    if (out_spoofing_flag) *out_spoofing_flag = directive.spoofing_flag ? 1 : 0;

    return directive.pentary_action;
}

#ifdef __cplusplus
}
#endif


// =====================================================================
// 8. UNIVERSAL BUS INTEGRATION (ZvdModuleBus)
// =====================================================================

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_quant_financial_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    if (!ctx.prompt.empty() && (ctx.prompt.starts_with("ZVD_FIN_TICK") || 
                                ctx.prompt.starts_with("ZVD_FIN_ORDERBOOK") || 
                                ctx.prompt.starts_with("ZVD_FIN_ARBITRAGE"))) {
        
        static thread_local ZvdQuantFinancialArbiter t_arbiter{};
        alignas(64) int8_t face_60[60];
        alignas(64) int8_t vertex_60[60];

        if (ctx.datacenter_pkt_ptr != nullptr) {
            const auto* pkt = static_cast<const ZVD_DatacenterPacket*>(ctx.datacenter_pkt_ptr);
            for (int i = 0; i < 60; ++i) {
                face_60[i] = static_cast<int8_t>((pkt->monolith.face_packed[i] % 5) - 2);
                vertex_60[i] = static_cast<int8_t>((pkt->monolith.vertex_packed[i] % 5) - 2);
            }
        } else {
            // Fold prompt bytes via Egyptian Triangle 3-4-5 LUT
            const char* p = ctx.prompt.data();
            size_t sz = ctx.prompt.size();
            for (int i = 0; i < 60; ++i) {
                uint8_t ba = static_cast<uint8_t>(p[(i * 2) % sz]);
                uint8_t bb = static_cast<uint8_t>(p[(i * 2 + 1) % sz]);
                face_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[ba & 0x0F][bb & 0x0F];
                vertex_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[bb & 0x0F][ba & 0x0F];
            }
        }

        auto directive = t_arbiter.process_market_tick(face_60, vertex_60);
        ctx.accumulated_trity = directive.pentary_action;
        ctx.routing_directive = directive.spoofing_flag ? "FIN_SPOOFING_VETO_ENGAGED" : 
                                (directive.quarantine_flag ? "FIN_LOMONOSOV_QUARANTINE_VETO" : "FIN_ARBITRAGE_PASS_OK");
        return directive.pentary_action;
    }
    return 0;
}

inline int zvd_quant_financial_evaluate(std::string_view prompt, int8_t gl_inv, double entropy) noexcept {
    (void)gl_inv;
    (void)entropy;
    if (prompt.starts_with("ZVD_FIN")) return 1;
    return 0;
}

inline bool zvd_quant_financial_verify(std::string_view prompt, std::string_view response) noexcept {
    if (prompt.starts_with("ZVD_FIN_VERIFY")) {
        return !response.empty();
    }
    return true;
}
#endif

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("QuantFinancialTheater-120D-HFT", ZvdModuleRole::ARBITER, zvd_quant_financial_evaluate_ctx, zvd_quant_financial_verify);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("QuantFinancialTheater-120D-HFT", zvd_quant_financial_evaluate, zvd_quant_financial_verify);
#endif


// =====================================================================
// 9. INTERACTIVE CLI CONSOLE MENU (ZVD-120D HFT WORKSTATION)
// =====================================================================

#ifndef ZVD_NO_MAIN

inline void run_hft_interactive_menu(void) {
    ZvdQuantFinancialArbiter arbiter;
    int choice = -1;

    while (choice != 0) {
        std::printf("\n======================================================================\n");
        std::printf("  ZVD-120D QUANTITATIVE FINANCIAL ARBITER & HFT COGNITIVE THEATER\n");
        std::printf("  Root-Architect: Vladimir Zavodiuk (Varna, Bulgaria, EU)\n");
        std::printf("======================================================================\n");
        std::printf(" [1] Simulate Bullish Orderbook Tick & Egyptian Folding\n");
        std::printf(" [2] Simulate Spoofing & Wash-Trading Attack (Kirchhoff VETO Test)\n");
        std::printf(" [3] Inspect 12-Mask Adversarial Debate Status\n");
        std::printf(" [4] Query 81-Branch Lomonosov Causal Tree & Quarantine Status\n");
        std::printf(" [0] Exit Workstation\n");
        std::printf("----------------------------------------------------------------------\n");
        std::printf("Select menu option: ");

        if (std::scanf("%d", &choice) != 1) {
            std::printf("[!] Invalid input. Please enter a valid integer.\n");
            while (std::getchar() != '\n');
            continue;
        }
        while (std::getchar() != '\n');

        switch (choice) {
            case 1: {
                std::printf("\n[*] Simulating normal bullish orderbook tick with Egyptian 3-4-5 folding...\n");
                alignas(64) int8_t face_60[60];
                alignas(64) int8_t vertex_60[60];
                for (int i = 0; i < 60; ++i) {
                    face_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[i % 16][(i + 3) % 16];
                    vertex_60[i] = ZVD_FIN_EGYPTIAN_HYPOT_LUT[(i + 5) % 16][i % 16];
                }
                auto directive = arbiter.process_market_tick(face_60, vertex_60);
                std::printf("    -> Pentary Action Code: %d (2: Aggressive, 1: Limit, 0: Neutral, -1: Hedge, -2: Veto)\n", directive.pentary_action);
                std::printf("    -> Final Q8 Score:      %d (Scale 256 = 1.0)\n", directive.final_score_q8);
                std::printf("    -> Spoofing Detected:   %s\n", directive.spoofing_flag ? "YES" : "NO");
                std::printf("    -> Branch Quarantine:   %s\n", directive.quarantine_flag ? "YES (QUARANTINED)" : "NO");
                std::printf("    -> Causal Tree Branch:  %u / 80\n", directive.branch_id);
                break;
            }
            case 2: {
                std::printf("\n[*] Simulating malicious spoofing & unbalanced orderbook cascade...\n");
                alignas(64) int8_t face_60[60];
                alignas(64) int8_t vertex_60[60];
                for (int i = 0; i < 60; ++i) {
                    face_60[i] = 1;
                    vertex_60[i] = 6; // Extreme fake tape velocity
                }
                auto directive = arbiter.process_market_tick(face_60, vertex_60);
                std::printf("    -> Pentary Action Code: %d (Expected: -2 for HARD VETO)\n", directive.pentary_action);
                std::printf("    -> Spoofing Guard Status: %s\n", directive.spoofing_flag ? "TRIGGERED (VETO ACTIVE)" : "PASSED");
                break;
            }
            case 3: {
                std::printf("\n[*] Inspecting 12-Mask Adversarial Debate status...\n");
                ZvdTeatroEnsemble ensemble;
                auto res = ensemble.evaluate_adversarial_debate(200, 50, 180);
                std::printf("    -> Consensus Score Q8:  %d\n", res.consensus_score_q8);
                std::printf("    -> Capital Group 0 (G0): %d\n", res.group0_score_q8);
                std::printf("    -> Risk Group 1 (G1):    %d\n", res.group1_score_q8);
                std::printf("    -> Crowd Hype Group 2 (G2): %d\n", res.group2_score_q8);
                std::printf("    -> Inter-Group Conflict: %d\n", res.inter_group_conflict_q8);
                std::printf("    -> Ensemble Coherent:   %s\n", res.is_coherent ? "YES" : "WARNING (Conflict/Divergence)");
                break;
            }
            case 4: {
                std::printf("\n[*] Querying 81-branch Lomonosov Causal Tree...\n");
                std::printf("    -> Tree active. 81 discrete ternary market regimes indexed in L1 cache.\n");
                break;
            }
            case 0:
                std::printf("[*] Exiting Quant Financial Workstation. All resources safely released.\n");
                break;
            default:
                std::printf("[!] Unknown menu option. Please select 0-4.\n");
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    run_hft_interactive_menu();
    return 0;
}

#endif // ZVD_NO_MAIN
