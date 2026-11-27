/* =============================================================================
 *  PROJECT:    ZVD COGNITIVE THEATRE 12-MASK & FEDERATED LOMONOSOV NEXUS (SLOT 2)
 *              (13.11_zvd_cognitive_theatre.hpp - V3.2.0-SUPREME-C20)
 *  FILENAME:   13.11_zvd_cognitive_theatre.hpp
 *  VERSION:    2026.11-V3.2.0-SUPREME-C20
 *
 *  AUTHOR:     Vladimir Zavodiuk | Root-Architect / Varna, Bulgaria
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk.
 *              ALL RIGHTS RESERVED.
 *
 *  --------------------------------------------------------------------------
 *  ENGINEERING NOTE: Technology Constructor Ecosystem V3.2 Module (SLOT 2)
 *  --------------------------------------------------------------------------
 *  Zero-Lock Thread-Local Monoliths & Zero-Alloc Stack Verification Hot-Paths.
 *  Build command (Linux/macOS):
 *   g++ -std=c++20 -O3 -march=native -Wall -Wextra -Werror -fopenmp "zvd_local_ai_proxy.cpp" -o zvd_proxy -lm
 * =============================================================================
 */

#pragma once

#define ZVD_HAS_COGNITIVE_AI 1

#if !defined(ZVD_HAS_DATACENTER_CORE) || !defined(ZVD_NO_MAIN)
#error "[ZVD-CONSTRUCTOR ERROR] Module zvd_cognitive_theatre.hpp (SLOT 2) cannot operate standalone! Requires simultaneous inclusion: zvd_local_ai_proxy.cpp and datacenter core."
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <algorithm>
#include <array>
#include <string_view>
#include <span>
#include <atomic>

// =====================================================================
// 0. RUNTIME CONFIG & ENTROPY GUARD HELPERS
// =====================================================================

namespace ZvdTheatreConfig {

    constexpr double DEFAULT_ENTROPY_LIMIT = 7.2;
    constexpr double MIN_ENTROPY_LIMIT = 4.0;
    constexpr double MAX_ENTROPY_LIMIT = 8.0;

    inline double get_cached_entropy_limit() noexcept {
        static const double limit = []() noexcept -> double {
            const char* env_lim = std::getenv("ZVD_THEATRE_ENTROPY_LIMIT");
            if (!env_lim || env_lim[0] == '\0') return DEFAULT_ENTROPY_LIMIT;
            errno = 0;
            char* endptr = nullptr;
            double val = std::strtod(env_lim, &endptr);
            if (errno != 0 || endptr == env_lim || *endptr != '\0' || !std::isfinite(val)) {
                return DEFAULT_ENTROPY_LIMIT;
            }
            return std::clamp(val, MIN_ENTROPY_LIMIT, MAX_ENTROPY_LIMIT);
        }();
        return limit;
    }

} // namespace ZvdTheatreConfig

// =====================================================================
// 1. NATIVE C++20 ZERO-ALLOC SHA-256 UTILITY
// =====================================================================

class ZvdNativeSHA256 {
private:
    static constexpr uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

    static void transform_block(const uint8_t* block, uint32_t& h0, uint32_t& h1, uint32_t& h2, uint32_t& h3,
                               uint32_t& h4, uint32_t& h5, uint32_t& h6, uint32_t& h7) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;
    }

public:
    static std::array<uint8_t, 32> hash(std::span<const uint8_t> data) {
        uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
        uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

        uint64_t orig_len = data.size() * 8;
        size_t full_blocks = data.size() / 64;
        for (size_t i = 0; i < full_blocks; ++i) {
            transform_block(data.data() + i * 64, h0, h1, h2, h3, h4, h5, h6, h7);
        }

        std::array<uint8_t, 128> tail_buf{};
        size_t rem = data.size() % 64;
        std::memcpy(tail_buf.data(), data.data() + full_blocks * 64, rem);
        tail_buf[rem] = 0x80;
        size_t tail_len = rem + 1;

        if (tail_len > 56) {
            std::memset(tail_buf.data() + tail_len, 0, 64 - tail_len);
            transform_block(tail_buf.data(), h0, h1, h2, h3, h4, h5, h6, h7);
            tail_len = 0;
        }
        std::memset(tail_buf.data() + tail_len, 0, 56 - tail_len);
        for (int i = 7; i >= 0; --i) {
            tail_buf[56 + (7 - i)] = static_cast<uint8_t>((orig_len >> (i * 8)) & 0xFF);
        }
        transform_block(tail_buf.data(), h0, h1, h2, h3, h4, h5, h6, h7);

        std::array<uint8_t, 32> digest;
        uint32_t h_arr[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
        for (int i = 0; i < 8; ++i) {
            digest[i * 4]     = static_cast<uint8_t>((h_arr[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((h_arr[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((h_arr[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>(h_arr[i] & 0xFF);
        }
        return digest;
    }
};

// =====================================================================
// 2. 1000-BYTE SUPREME MONOLITH & TETRACUBE (THREAD-LOCAL ZERO-LOCK)
// =====================================================================

struct alignas(64) ZvdSupremeMonolith1000B {
    int8_t face_residual[60];
    int8_t face_bruns[60];
    int16_t face_weight[60];
    int16_t face_tension[60];
    uint16_t face_phase_sync[60];

    int8_t vertex_residual[60];
    int8_t vertex_bruns[60];
    int16_t vertex_weight[60];
    int16_t vertex_tension[60];
    uint16_t vertex_phase_sync[60];

    uint8_t padding[40];

    ZvdSupremeMonolith1000B() { reset(); }

    void reset() {
        std::memset(face_residual, 0, sizeof(face_residual));
        std::memset(face_bruns, 0, sizeof(face_bruns));
        std::memset(face_weight, 0, sizeof(face_weight));
        std::memset(face_tension, 0, sizeof(face_tension));
        std::memset(face_phase_sync, 0, sizeof(face_phase_sync));

        std::memset(vertex_residual, 0, sizeof(vertex_residual));
        std::memset(vertex_bruns, 0, sizeof(vertex_bruns));
        std::memset(vertex_weight, 0, sizeof(vertex_weight));
        std::memset(vertex_tension, 0, sizeof(vertex_tension));
        std::memset(vertex_phase_sync, 0, sizeof(vertex_phase_sync));
        std::memset(padding, 0, sizeof(padding));
    }

    std::array<uint8_t, 1000> pack_to_buffer() const {
        std::array<uint8_t, 1000> out_buffer{};
        size_t off = 0;
        std::memcpy(out_buffer.data() + off, face_residual, sizeof(face_residual)); off += sizeof(face_residual);
        std::memcpy(out_buffer.data() + off, face_bruns, sizeof(face_bruns)); off += sizeof(face_bruns);
        std::memcpy(out_buffer.data() + off, face_weight, sizeof(face_weight)); off += sizeof(face_weight);
        std::memcpy(out_buffer.data() + off, face_tension, sizeof(face_tension)); off += sizeof(face_tension);
        std::memcpy(out_buffer.data() + off, face_phase_sync, sizeof(face_phase_sync)); off += sizeof(face_phase_sync);

        off = 500;
        std::memcpy(out_buffer.data() + off, vertex_residual, sizeof(vertex_residual)); off += sizeof(vertex_residual);
        std::memcpy(out_buffer.data() + off, vertex_bruns, sizeof(vertex_bruns)); off += sizeof(vertex_bruns);
        std::memcpy(out_buffer.data() + off, vertex_weight, sizeof(vertex_weight)); off += sizeof(vertex_weight);
        std::memcpy(out_buffer.data() + off, vertex_tension, sizeof(vertex_tension)); off += sizeof(vertex_tension);
        std::memcpy(out_buffer.data() + off, vertex_phase_sync, sizeof(vertex_phase_sync)); off += sizeof(vertex_phase_sync);
        return out_buffer;
    }
};

inline ZvdSupremeMonolith1000B& get_thread_monolith1000b() {
    thread_local ZvdSupremeMonolith1000B thread_monolith;
    return thread_monolith;
}

// =====================================================================
// 3. FEDERATED LOMONOSOV EXPERIENCE NEXUS (81 branches 3^4)
// =====================================================================

struct FederatedLomonosovBranch {
    std::atomic<uint32_t> visit_count{0};
    std::atomic<double> cumulative_reward{0.0};
    std::atomic<int8_t> learned_trity{0};
};

inline std::array<FederatedLomonosovBranch, 81>& get_federated_lomonosov_tree() {
    static std::array<FederatedLomonosovBranch, 81> tree;
    return tree;
}

// =====================================================================
// 4. COGNITIVE THEATRE 12 MASKS & 4-STAGE ARBITRATION PIPELINE
// =====================================================================

class ZvdCognitiveTheatre120D {
private:
    ZVD_BiCore120D bicore_;
    alignas(64) std::array<float, ZVD_DIM_120> collapsed_120_{};
    alignas(64) std::array<int8_t, ZVD_DIM_120> synth_120_{};

public:
    ZvdCognitiveTheatre120D() {
        zvd_ensure_luts();
        zvd_bicore_init(&bicore_, 0, 1);
    }

    struct ArbitrationResult {
        int verdict;
        std::string routing_directive;
        double confidence_score;
    };

    ArbitrationResult arbitrate_full(std::string_view prompt, int8_t gl_invariant, double prompt_entropy, bool record_stats = true) {
        if (prompt.empty()) return {0, "FAST_CONCISE", 0.5};

        const double entropy_limit = ZvdTheatreConfig::get_cached_entropy_limit();
        if (!std::isfinite(prompt_entropy) || prompt_entropy > entropy_limit || gl_invariant <= -2) {
            return {-1, "THEATRE_VETO", 0.0};
        }

        zvd_bicore_update_rotation(&bicore_, prompt_entropy);
        zvd_stride_collapse_to_120(reinterpret_cast<const uint8_t*>(prompt.data()), prompt.size(), collapsed_120_.data());
        zvd_enterprise_synthesis_120d(collapsed_120_.data(), &bicore_, synth_120_.data());

        ZvdSupremeMonolith1000B& monolith = get_thread_monolith1000b();

        alignas(64) std::array<double, ZVD_DIM_120> modulated_synth{};
        for (int i = 0; i < ZVD_DIM_120; ++i) {
            double base_val = static_cast<double>(synth_120_[i]);
            double w_factor = (i < 60) ? static_cast<double>(monolith.face_weight[i]) / 5000.0
                                       : static_cast<double>(monolith.vertex_weight[i - 60]) / 5000.0;
            modulated_synth[i] = base_val * (1.0 + std::clamp(w_factor, -0.5, 0.8));
        }

        // Stage 1: 12 Masks (Pythagorean 3-4-5)
        alignas(64) std::array<int8_t, 12> masks_verdicts{};
        alignas(64) std::array<double, 12> masks_scores{};
        for (int m = 0; m < 12; ++m) {
            double sum_a = 0.0, sum_b = 0.0;
            for (int f = 0; f < 5; ++f) {
                sum_a += modulated_synth[m * 10 + f];
                sum_b += modulated_synth[m * 10 + 5 + f];
            }
            double leg_3 = sum_a * 3.0;
            double leg_4 = sum_b * 4.0;
            double hypot_sq = leg_3 * leg_3 + leg_4 * leg_4;

            double x = (sum_a + sum_b) * 0.25;
            masks_scores[m] = 0.5 + 0.5 * (x / (1.0 + std::abs(x)));

            int8_t v = 0;
            if (hypot_sq >= 25.0 && (sum_a + sum_b) > 1.5) v = 1;
            else if (hypot_sq >= 25.0 && (sum_a + sum_b) < -1.5) v = -1;
            masks_verdicts[m] = v;
        }

        // Stage 2: 4 Rashomon Groups
        alignas(64) std::array<int8_t, 4> group_verdicts{};
        alignas(64) std::array<double, 4> group_hypots{};
        for (int g = 0; g < 4; ++g) {
            double v0 = static_cast<double>(masks_verdicts[g * 3]);
            double v1 = static_cast<double>(masks_verdicts[g * 3 + 1]);
            double v2 = static_cast<double>(masks_verdicts[g * 3 + 2]);

            double grp_leg_3 = (v0 + v1) * 3.0;
            double grp_leg_4 = v2 * 4.0;
            double grp_hypot_sq = grp_leg_3 * grp_leg_3 + grp_leg_4 * grp_leg_4;
            group_hypots[g] = grp_hypot_sq;

            int8_t g_v = 0;
            double g_sum = v0 + v1 + v2;
            if (grp_hypot_sq >= 9.0 && g_sum >= 1.0) g_v = 1;
            else if (grp_hypot_sq >= 9.0 && g_sum <= -1.0) g_v = -1;
            group_verdicts[g] = g_v;
        }

        std::string routing = "FAST_CONCISE";
        if (group_verdicts[0] == 0 || group_verdicts[0] == -1) routing = "DEEP_COT";

        bool tension_exceeded = (std::abs(monolith.face_tension[5]) > 250);
        if (tension_exceeded || group_hypots[1] > 144.0) routing += " + WEB_SEARCH_REQUIRED";

        // Stage 3: Counter-Rotating Moire 6 vs 6
        int coincidence_nodes = 0, conflict_nodes = 0;
        for (int k = 0; k < ZVD_DIM_60; ++k) {
            int f_val = static_cast<int>(synth_120_[k]);
            int v_val = static_cast<int>(synth_120_[60 + k]);

            int counter_phase = ((k * 3 + bicore_.angle_deg_a) % 360 - (k * 3 + bicore_.angle_deg_b) % 360 + 360) % 360;
            if (f_val * v_val > 0 && (counter_phase <= 90 || counter_phase >= 270)) {
                coincidence_nodes++;
                if (record_stats) {
                    monolith.face_phase_sync[k] = static_cast<uint16_t>(std::min(65535, static_cast<int>(monolith.face_phase_sync[k]) + 1));
                }
            } else if (f_val * v_val < 0) {
                conflict_nodes++;
            }

            int16_t t_val = static_cast<int16_t>((f_val - v_val) * 100);
            if (record_stats) {
                monolith.face_tension[k] = t_val;
                monolith.vertex_tension[k] = static_cast<int16_t>(-t_val);
                monolith.face_residual[k] = static_cast<int8_t>(f_val);
                monolith.vertex_residual[k] = static_cast<int8_t>(v_val);
            }
        }

        // Stage 4: Federated Lomonosov Tree & Captain Final Verdict
        int g0 = group_verdicts[0], g1 = group_verdicts[1], g2 = group_verdicts[2], g3 = group_verdicts[3];
        int branch_idx = std::clamp((g0 + 1) * 27 + (g1 + 1) * 9 + (g2 + 1) * 3 + (g3 + 1), 0, 80);

        auto& tree = get_federated_lomonosov_tree();
        FederatedLomonosovBranch& branch = tree[branch_idx];

        uint32_t v_count = branch.visit_count.load(std::memory_order_relaxed);
        double c_reward = branch.cumulative_reward.load(std::memory_order_relaxed);

        bool branch_quarantined = (v_count >= 2 && c_reward < -0.075);

        int capitano_vote = masks_verdicts[2];
        double group_sum_all = static_cast<double>(g0 + g1 + g2 + g3);
        double node_advantage = static_cast<double>(coincidence_nodes - conflict_nodes) * 0.25;

        double cap_leg_3 = group_sum_all * 3.0;
        double cap_leg_4 = (node_advantage + static_cast<double>(capitano_vote) * 2.0) * 4.0;
        double capitano_hypot_sq = cap_leg_3 * cap_leg_3 + cap_leg_4 * cap_leg_4;

        double score_sum = 0.0;
        for (double s : masks_scores) score_sum += s;
        double combined_score = score_sum / 12.0;

        int final_verdict = 0;
        if (gl_invariant <= -2 || !std::isfinite(prompt_entropy) || prompt_entropy > entropy_limit || capitano_vote == -1 || branch_quarantined) {
            final_verdict = -1;
        } else if (capitano_hypot_sq >= 25.0 && group_sum_all >= 1.0 && capitano_vote >= 0) {
            final_verdict = 1;
        } else {
            final_verdict = 0;
        }

        if (record_stats) {
            branch.visit_count.fetch_add(1, std::memory_order_relaxed);
            double old_rew = branch.cumulative_reward.load(std::memory_order_relaxed);
            double new_rew = 0.0;

            if (branch_quarantined) {
                new_rew = old_rew + 0.015;
                while (!branch.cumulative_reward.compare_exchange_weak(old_rew, new_rew, std::memory_order_relaxed)) {
                    new_rew = old_rew + 0.015;
                }
            } else {
                double reward_delta = static_cast<double>(final_verdict) * 0.5;
                auto calc_rew = [&](double cur) noexcept -> double {
                    if (cur < 0.0 && gl_invariant != -2 && prompt_entropy <= entropy_limit && capitano_vote >= 0) {
                        return (cur * 0.85) + (0.15 * reward_delta);
                    }
                    return 0.9 * cur + 0.1 * reward_delta;
                };
                new_rew = calc_rew(old_rew);
                while (!branch.cumulative_reward.compare_exchange_weak(old_rew, new_rew, std::memory_order_relaxed)) {
                    new_rew = calc_rew(old_rew);
                }
            }
            branch.learned_trity.store(static_cast<int8_t>(final_verdict), std::memory_order_relaxed);

            for (int i = 0; i < ZVD_DIM_60; ++i) {
                double error = static_cast<double>(final_verdict) - modulated_synth[i];
                int new_f_w = static_cast<int>(monolith.face_weight[i]) + static_cast<int>(error * 50.0);
                int new_v_w = static_cast<int>(monolith.vertex_weight[i]) - static_cast<int>(error * 50.0);

                monolith.face_weight[i] = static_cast<int16_t>(std::clamp(new_f_w, -16000, 16000));
                monolith.vertex_weight[i] = static_cast<int16_t>(std::clamp(new_v_w, -16000, 16000));
            }
        }

        return {final_verdict, routing, combined_score};
    }
};

// =====================================================================
// 5. INTERFACE FUNCTIONS & UNIFIED BUS REGISTRATION (V3.2.0)
// =====================================================================

inline int zvd_ai_module_evaluate(std::string_view prompt, int8_t gl_invariant, double prompt_entropy) {
    thread_local ZvdCognitiveTheatre120D tl_theatre;
    auto res = tl_theatre.arbitrate_full(prompt, gl_invariant, prompt_entropy, true);
    return res.verdict;
}

inline int zvd_ai_module_evaluate_ctx(ZvdPipelineContext& ctx) {
    thread_local ZvdCognitiveTheatre120D tl_theatre;
    std::string_view prompt = !ctx.prompt.empty() ? ctx.prompt : std::string_view("");
    auto res = tl_theatre.arbitrate_full(prompt, ctx.gl_invariant, ctx.entropy, true);
    ctx.routing_directive = res.routing_directive;
    return res.verdict;
}

inline bool zvd_ai_module_verify_response(std::string_view prompt, std::string_view response) {
    if (response.empty()) return false;
    if (response.find("ZVD-VETO") != std::string_view::npos) return false;

    thread_local ZvdCognitiveTheatre120D tl_theatre;
    alignas(64) std::array<char, 4096> stack_buf{};

    constexpr size_t MAX_PROMPT_BYTES = 1024u;
    constexpr size_t MAX_HALF_RESPONSE_BYTES = 1024u;
    constexpr const char* SEP_RESP = "\n---RESPONSE---\n";
    constexpr const char* SEP_TAIL = "\n---TAIL---\n";
    constexpr const char* REF_TAG = "\n[ZVD-VERIFIED-C20]";

    const size_t sep_resp_len = std::strlen(SEP_RESP);
    const size_t sep_tail_len = std::strlen(SEP_TAIL);
    const size_t ref_tag_len = std::strlen(REF_TAG);

    const size_t p_len = std::min(prompt.size(), MAX_PROMPT_BYTES);
    const bool use_head_tail = response.size() > 2048u;
    const size_t resp_payload_len = use_head_tail
        ? (MAX_HALF_RESPONSE_BYTES + sep_tail_len + MAX_HALF_RESPONSE_BYTES)
        : std::min(response.size(), MAX_HALF_RESPONSE_BYTES * 2u);

    const size_t needed_with_two_tags = p_len + sep_resp_len + resp_payload_len + (ref_tag_len * 2u);
    if (needed_with_two_tags > stack_buf.size()) {
        return true;
    }

    size_t off = 0;
    if (p_len > 0 && prompt.data() != nullptr) {
        std::memcpy(stack_buf.data() + off, prompt.data(), p_len);
        off += p_len;
    }

    std::memcpy(stack_buf.data() + off, SEP_RESP, sep_resp_len);
    off += sep_resp_len;

    if (use_head_tail) {
        std::memcpy(stack_buf.data() + off, response.data(), MAX_HALF_RESPONSE_BYTES);
        off += MAX_HALF_RESPONSE_BYTES;

        std::memcpy(stack_buf.data() + off, SEP_TAIL, sep_tail_len);
        off += sep_tail_len;

        const size_t tail_offset = response.size() - MAX_HALF_RESPONSE_BYTES;
        std::memcpy(stack_buf.data() + off, response.data() + tail_offset, MAX_HALF_RESPONSE_BYTES);
        off += MAX_HALF_RESPONSE_BYTES;
    } else {
        const size_t r_len = response.size();
        if (r_len > 0 && response.data() != nullptr) {
            std::memcpy(stack_buf.data() + off, response.data(), r_len);
            off += r_len;
        }
    }

    for (int cycle = 0; cycle < 3; ++cycle) {
        std::string_view composite_stream(stack_buf.data(), off);
        double entropy = zvd_shannon_entropy(
            reinterpret_cast<const uint8_t*>(composite_stream.data()),
            composite_stream.size()
        );

        auto res = tl_theatre.arbitrate_full(composite_stream, 0, entropy, false);
        if (res.verdict == -1) return false;

        if (cycle < 2 && off + ref_tag_len <= stack_buf.size()) {
            std::memcpy(stack_buf.data() + off, REF_TAG, ref_tag_len);
            off += ref_tag_len;
        }
    }

    return true;
}

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
ZVD_REGISTER_CARTRIDGE_CTX("CognitiveTheatre-120D-Lomonosov", ZvdModuleRole::ARBITER, zvd_ai_module_evaluate_ctx, zvd_ai_module_verify_response);
#elif defined(ZVD_REGISTER_CARTRIDGE)
ZVD_REGISTER_CARTRIDGE("CognitiveTheatre-120D-Lomonosov", zvd_ai_module_evaluate, zvd_ai_module_verify_response);
#endif
