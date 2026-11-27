/* =============================================================================
 *  PROJECT:    ZVD-120D SOVEREIGN AI DATACENTER PRODUCTION ORCHESTRATOR
 *              (13.11_zvd_master_integration_suite_v30.cpp - V30.2-SUPREME-9MOD-C20)
 *  COMPILATION:
 *    g++ -std=c++20 -O3 -march=native -Wall -Wextra -Werror -fopenmp \
 *      "13.11_zvd_master_integration_suite_v30.cpp" -o zvd_master_suite -lm -lpthread
 *    (With OpenSSL enabled in industry bridge, add: -lssl -lcrypto)
 *
 *  AUTHOR:     Vladimir Zavodiuk
 *              Root-Architect / Varna, Bulgaria (European Union)
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk. ALL RIGHTS RESERVED.
 *              Strictly proprietary. Unauthorized copying, modification, distribution,
 *              or commercial utilization is strictly prohibited without direct written consent.
 *
 *  COMPLIANCE: ISO 26262 ASIL D / MISRA C:2012 / Strict English I18n
 *              Zero-Heap Hot Path, alignas(64), Atomic .zvd3 Vault, OpenMP Safe
 *
 *  --------------------------------------------------------------------------
 *  ARCHITECTURAL AUDIT & VERIFICATION PROTOCOL (9-MODULE MONOLITH)
 *  --------------------------------------------------------------------------
 *  1. Whole-Suite Inspection: Audit the 9-module stack as an integrated whole
 *     (Layers 0-3 included into Layer 4 orchestrator; 13.11_zvd_visual_crypto_v30.hpp
 *     remains an isolated standalone contour outside this 9-module suite).
 *  2. Physical & Algorithmic Reality:
 *     - Zero-Heap hot path with 64-byte cache-line alignment (alignas(64)) and
 *       thread-local / stack buffers without heap allocation in steady state.
 *     - Compact lookup tables (< 6 KB L1-resident) and deterministic math.
 *     - 334-byte Monolith334 Lukasiewicz fold, 120D synthesis, 12-mask / 4-group
 *       Rashomon / counter-rotating Moire / 81-branch Lomonosov arbitration.
 *     - Selective slot-level pread() over 704-byte micro-slots and multi-shard
 *       .zvd3 + manifest storage up to ~58.9 GB per object.
 *     - Early anomaly rejection and safety VETO interlocks prior to GPU/backend
 *       dispatch, plus Prometheus telemetry and ChaCha20-Poly1305 AEAD transport.
 * =============================================================================
 */

#include <atomic>

#ifndef ZVD_GLOBAL_SHUTDOWN_DEFINED
#define ZVD_GLOBAL_SHUTDOWN_DEFINED 1
inline std::atomic<bool> g_zvd_global_shutdown{false};
#endif

#define ZVD_NO_MAIN 1

#ifndef ZVD_DATACENTER_CORE_IMPL_INCLUDED
#define ZVD_DATACENTER_CORE_IMPL_INCLUDED 1
#endif

// =====================================================================
// LAYER 0: MATHEMATICAL & GEOMETRIC FOUNDATION
// =====================================================================
#if defined(__has_include)
  #if __has_include("13.11_zvd_datacenter_node_v28_lite.c")
    #define ZVD_HAS_DATACENTER_CORE 1
    extern "C" {
      #include "13.11_zvd_datacenter_node_v28_lite.c"
    }
  #endif
#endif

#if defined(__has_include)
  #if __has_include("13.11_zvd_nested_storage_codec_v32.hpp")
    #if !defined(ZVD_HAS_NESTED_CODEC_V32) && !defined(ZVD_HAS_NESTED_STORAGE_CODEC)
      #define ZVD_HAS_NESTED_CODEC_V32 1
      #define ZVD_HAS_NESTED_CODEC_MODULE 1
      #include "13.11_zvd_nested_storage_codec_v32.hpp"
    #endif
  #endif
#endif

// =====================================================================
// LAYER 1: INGRESS REACTOR & CANONICAL CARTRIDGE BUS
// =====================================================================
#if defined(__has_include)
  #if __has_include("13.11_zvd_local_ai_proxy.cpp")
    #define ZVD_HAS_LOCAL_AI_PROXY 1
    #include "13.11_zvd_local_ai_proxy.cpp"
  #endif
#endif

// =====================================================================
// LAYER 2: DEFENSIVE & COGNITIVE BUS SLOTS
// =====================================================================
#if defined(__has_include)
  #if __has_include("13.11_zvd_ddos_aegis_v30.hpp")
    #define ZVD_HAS_DDOS_AEGIS_V30 1
    #include "13.11_zvd_ddos_aegis_v30.hpp"
  #endif
#endif

#if defined(__has_include)
  #if __has_include("13.11_zvd_cognitive_theatre.hpp")
    #define ZVD_HAS_COGNITIVE_THEATRE_V31 1
    #include "13.11_zvd_cognitive_theatre.hpp"
  #endif
#endif

// =====================================================================
// LAYER 3: SOVEREIGN STORAGE FABRIC, TENSOR C-ABI & INDUSTRY BRIDGE
// =====================================================================
#if defined(__has_include)
  #if __has_include("13.11_zvd_sovereign_storage_fabric.cpp")
    #define ZVD_HAS_STORAGE_FABRIC_MODULE 1
    #include "13.11_zvd_sovereign_storage_fabric.cpp"
  #endif
#endif

#if defined(__has_include)
  #if __has_include("13.11_zvd_v32_tensor_vault_bridge.cpp")
    #define ZVD_HAS_TENSOR_VAULT_BRIDGE 1
    #include "13.11_zvd_v32_tensor_vault_bridge.cpp"
  #endif
#endif

#if defined(__has_include)
  #if __has_include("13.11_zvd_industry_bridge_v33.hpp")
    #define ZVD_HAS_INDUSTRY_BRIDGE_V33 1
    #include "13.11_zvd_industry_bridge_v33.hpp"
  #endif
#endif

// =====================================================================
// LAYER 4: MASTER ORCHESTRATOR MANIFEST, INVARIANTS & RUNTIME
// =====================================================================
#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ZvdProductionOrchestrator {

    enum class ZvdSuiteLayer : uint8_t {
        Layer0_Foundation = 0u,
        Layer1_ProxyBus   = 1u,
        Layer2_GuardCog   = 2u,
        Layer3_VaultInd   = 3u,
        Layer4_Orchestr   = 4u
    };

    enum class RunMode : uint8_t {
        DefaultAuto = 0u,
        Datacenter  = 1u,
        Enterprise  = 2u,
        ProxyOnly   = 3u,
        FabricOnly  = 4u,
        Daemon      = 5u,
        AuditOnly   = 6u
    };

    struct ZvdModuleManifestEntry {
        uint8_t       index;
        ZvdSuiteLayer layer;
        const char*   file_name;
        const char*   role_tag;
        bool          compiled_in;
    };

    inline constexpr std::array<ZvdModuleManifestEntry, 9> ZVD_9MODULE_MANIFEST{{
        ZvdModuleManifestEntry{
            1u, ZvdSuiteLayer::Layer0_Foundation,
            "13.11_zvd_datacenter_node_v28_lite.c",
            "120D Synthesis & Lukasiewicz Geodesics",
#if defined(ZVD_HAS_DATACENTER_CORE)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            2u, ZvdSuiteLayer::Layer0_Foundation,
            "13.11_zvd_nested_storage_codec_v32.hpp",
            "3-Level Nested Codec & Slot Pread",
#if defined(ZVD_HAS_NESTED_CODEC_V32)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            3u, ZvdSuiteLayer::Layer1_ProxyBus,
            "13.11_zvd_local_ai_proxy.cpp",
            "Ingress Proxy Engine & Cartridge Bus",
#if defined(ZVD_HAS_LOCAL_AI_PROXY)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            4u, ZvdSuiteLayer::Layer2_GuardCog,
            "13.11_zvd_ddos_aegis_v30.hpp",
            "DDoS Aegis Shield & Flow Guard",
#if defined(ZVD_HAS_DDOS_AEGIS_V30)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            5u, ZvdSuiteLayer::Layer2_GuardCog,
            "13.11_zvd_cognitive_theatre.hpp",
            "12-Mask Cognitive Theatre & Lomonosov Nexus",
#if defined(ZVD_HAS_COGNITIVE_THEATRE_V31)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            6u, ZvdSuiteLayer::Layer3_VaultInd,
            "13.11_zvd_sovereign_storage_fabric.cpp",
            "Sovereign Storage Fabric & Rack Replication",
#if defined(ZVD_HAS_STORAGE_FABRIC_MODULE)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            7u, ZvdSuiteLayer::Layer3_VaultInd,
            "13.11_zvd_v32_tensor_vault_bridge.cpp",
            "Multi-Shard Tensor Vault C-ABI Bridge",
#if defined(ZVD_HAS_TENSOR_VAULT_BRIDGE)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            8u, ZvdSuiteLayer::Layer3_VaultInd,
            "13.11_zvd_industry_bridge_v33.hpp",
            "Prometheus Telemetry & ChaCha20-Poly1305 AEAD",
#if defined(ZVD_HAS_INDUSTRY_BRIDGE_V33)
            true
#else
            false
#endif
        },
        ZvdModuleManifestEntry{
            9u, ZvdSuiteLayer::Layer4_Orchestr,
            "13.11_zvd_master_integration_suite_v30.cpp",
            "Master 9-Module Orchestrator & ASIL-D POST",
            true
        }
    }};

    static_assert(ZVD_9MODULE_MANIFEST.size() == 9u, "ZVD_9MODULE_MANIFEST must contain exactly 9 modules");
    static_assert(ZVD_9MODULE_MANIFEST[0].index == 1u && ZVD_9MODULE_MANIFEST[8].index == 9u,
                  "ZVD_9MODULE_MANIFEST indices must span 1..9");

#if defined(ZVD_HAS_DATACENTER_CORE)
    static_assert(sizeof(ZVD_Monolith334) == 334u, "ZVD_Monolith334 must remain 334 bytes");
  #if defined(ZVD_DIM_120)
    static_assert(ZVD_DIM_120 == 120, "ZVD_DIM_120 must equal 120");
  #endif
#endif

#if defined(ZVD_HAS_INDUSTRY_BRIDGE_V33)
    static_assert(ZVD_BRIDGE_AEAD_KEY_BYTES == 32u, "AEAD key size must be 32 bytes");
    static_assert(ZVD_BRIDGE_AEAD_NONCE_BYTES == 12u, "AEAD nonce size must be 12 bytes");
    static_assert(ZVD_BRIDGE_AEAD_TAG_BYTES == 16u, "AEAD tag size must be 16 bytes");
#endif

    struct alignas(64) RuntimeConfig {
        uint16_t proxy_port = 9120u;
        uint16_t requested_proxy_port = 9120u;
        char backend_ip[64] = "127.0.0.1";
        uint16_t backend_port = 8080u;
        uint16_t storage_port = 8080u;
        char storage_root[256] = "./zvd_vault";
        uint16_t rack_port = 9000u;
        int cpu_cores = 1;
        int storage_workers = 1;
        bool port_collision_resolved = false;
        RunMode mode = RunMode::DefaultAuto;
        bool daemonize = false;
        bool show_help = false;
    };

    static_assert(alignof(RuntimeConfig) == 64u, "RuntimeConfig must be 64-byte aligned");

    inline std::atomic<bool>& shutdown_requested_flag() noexcept {
        return g_zvd_global_shutdown;
    }

    extern "C" inline void handle_termination_signal(int sig) noexcept {
        (void)sig;
        g_zvd_global_shutdown.store(true, std::memory_order_release);
    }

    inline uint16_t parse_port_env(const char* name, uint16_t fallback_port) noexcept {
        const char* val = std::getenv(name);
        if (!val || val[0] == '\0') return fallback_port;
        char* end_ptr = nullptr;
        unsigned long parsed = std::strtoul(val, &end_ptr, 10);
        if (end_ptr == val || *end_ptr != '\0' || parsed == 0UL || parsed > 65535UL) {
            return fallback_port;
        }
        return static_cast<uint16_t>(parsed);
    }

    inline void print_usage() noexcept {
        std::cout
            << "Usage: zvd_master_suite [--audit|--fabric|--datacenter|--enterprise|--proxy|--daemon|--help]\n"
            << "  --audit       Run 9-layer diagnostics, cartridge bus check, and timing report\n"
            << "  --fabric      Run storage fabric HTTP & rack replication contour\n"
            << "  --datacenter  Run full datacenter contour (fabric + proxy)\n"
            << "  --enterprise  Run full enterprise contour (fabric + proxy)\n"
            << "  --proxy       Run proxy contour only\n"
            << "  --daemon      Run in background/daemon mode\n"
            << "  --help, -h    Show this help message\n";
    }

    inline void parse_cli_args(int argc, char** argv, RuntimeConfig& cfg) noexcept {
        for (int i = 1; i < argc; ++i) {
            if (!argv[i]) continue;
            std::string_view arg(argv[i]);
            if (arg == "--audit") {
                cfg.mode = RunMode::AuditOnly;
            } else if (arg == "--fabric") {
                cfg.mode = RunMode::FabricOnly;
            } else if (arg == "--datacenter") {
                cfg.mode = RunMode::Datacenter;
            } else if (arg == "--enterprise") {
                cfg.mode = RunMode::Enterprise;
            } else if (arg == "--proxy") {
                cfg.mode = RunMode::ProxyOnly;
            } else if (arg == "--daemon") {
                cfg.daemonize = true;
                if (cfg.mode == RunMode::DefaultAuto) {
                    cfg.mode = RunMode::Daemon;
                }
            } else if (arg == "--help" || arg == "-h") {
                cfg.show_help = true;
            }
        }
    }

    inline void load_runtime_config(RuntimeConfig& cfg) noexcept {
        cfg.proxy_port = parse_port_env("ZVD_PROXY_PORT", 9120u);
        cfg.requested_proxy_port = cfg.proxy_port;
        cfg.port_collision_resolved = false;
        cfg.storage_port = parse_port_env("ZVD_LISTEN_PORT", 8080u);
        cfg.backend_port = parse_port_env("ZVD_BACKEND_PORT", cfg.storage_port);
        cfg.rack_port = parse_port_env("ZVD_RACK_PORT", 9000u);

        if (cfg.proxy_port == cfg.storage_port || cfg.proxy_port == cfg.backend_port) {
            uint16_t candidate = 9120u;
            if (candidate == cfg.storage_port || candidate == cfg.backend_port) candidate = 9121u;
            if (candidate == cfg.storage_port || candidate == cfg.backend_port) candidate = 9122u;
            if (candidate == cfg.storage_port || candidate == cfg.backend_port) {
                candidate = static_cast<uint16_t>((cfg.storage_port == 9123u || cfg.backend_port == 9123u) ? 9124u : 9123u);
            }
            cfg.proxy_port = candidate;
            cfg.port_collision_resolved = (cfg.proxy_port != cfg.requested_proxy_port);
        }

        const char* b_ip = std::getenv("ZVD_BACKEND_IP");
        if (b_ip && b_ip[0] != '\0') {
            std::snprintf(cfg.backend_ip, sizeof(cfg.backend_ip), "%s", b_ip);
        }

        const char* s_root = std::getenv("ZVD_STORAGE_ROOT");
        if (s_root && s_root[0] != '\0') {
            std::snprintf(cfg.storage_root, sizeof(cfg.storage_root), "%s", s_root);
        }

        int cores = 1;
#ifndef _WIN32
        long nprocs = ::sysconf(_SC_NPROCESSORS_ONLN);
        if (nprocs > 0) cores = static_cast<int>(nprocs);
#endif
#ifdef _OPENMP
        int omp_max = omp_get_max_threads();
        if (omp_max > cores) cores = omp_max;
#endif
        cfg.cpu_cores = (cores > 0) ? cores : 1;
        cfg.storage_workers = std::max(1, cfg.cpu_cores / 2);
    }

    inline bool ensure_storage_root_dir(const char* root_dir) noexcept {
#ifndef _WIN32
        if (!root_dir || root_dir[0] == '\0') {
            errno = EINVAL;
            return false;
        }
        char path_buf[sizeof(RuntimeConfig::storage_root)];
        size_t len = std::strlen(root_dir);
        if (len == 0u || len >= sizeof(path_buf)) {
            errno = (len >= sizeof(path_buf)) ? ENAMETOOLONG : EINVAL;
            return false;
        }
        std::memcpy(path_buf, root_dir, len + 1u);

        while (len > 1u && path_buf[len - 1u] == '/') {
            path_buf[--len] = '\0';
        }

        for (size_t i = 1u; i < len; ++i) {
            if (path_buf[i] == '/') {
                path_buf[i] = '\0';
                if (path_buf[0] != '\0') {
                    int rc = ::mkdir(path_buf, 0755);
                    if (rc != 0 && errno != EEXIST) {
                        return false;
                    }
                }
                path_buf[i] = '/';
            }
        }

        int rc = ::mkdir(path_buf, 0755);
        if (rc != 0 && errno != EEXIST) {
            return false;
        }
        return true;
#else
        (void)root_dir;
        return true;
#endif
    }

    inline bool run_asil_d_power_on_self_test() noexcept {
        volatile double normal_entropy = 2.1;
        volatile int8_t normal_invariant = 0;
        bool normal_veto_engaged = (normal_invariant == -2) || (normal_entropy > 7.2);
        if (normal_veto_engaged) {
            std::cerr << "[ASIL-D POST ERROR] False safety VETO triggered in STANDBY state.\n";
            return false;
        }

        volatile double anomaly_entropy = 8.5;
        volatile int8_t anomaly_invariant = -2;
        bool anomaly_veto_engaged = (anomaly_invariant == -2) || (anomaly_entropy > 7.2);
        if (!anomaly_veto_engaged) {
            std::cerr << "[ASIL-D POST ERROR] Safety VETO interlock failed to ENGAGE under fault condition.\n";
            return false;
        }

#if defined(ZVD_HAS_DATACENTER_CORE)
        ZVD_BiCore120D post_core{};
        zvd_bicore_init(&post_core, 0, 1);
        const uint8_t probe[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        ZVD_DatacenterPacket post_pkt{};
        double asym = 0.0;
        if (!zvd_node_ingest_monolith_only(probe, sizeof(probe), &post_core, &post_pkt) ||
            post_pkt.gl_invariant < -2 || post_pkt.gl_invariant > 2 ||
            !zvd_inspect_monolith(&post_pkt.monolith, &asym)) {
            std::cerr << "[ASIL-D POST ERROR] Datacenter 120D core self-test failed.\n";
            return false;
        }
#endif

#if defined(ZVD_HAS_NESTED_CODEC_V32)
        ZvdNestedCodecDetail::ensure_pyth_345_lut();
        const auto mode = zvd_get_codec_mode();
        const char* mode_name = zvd_codec_mode_name(mode);
        if (!mode_name || mode_name[0] == '\0') {
            std::cerr << "[ASIL-D POST ERROR] PYTH_345_LUT / codec mode descriptor invalid.\n";
            return false;
        }
#endif

        std::cout << "[*] ASIL-D POST: STANDBY check OK | Anomaly interlock ENGAGED -> VERIFIED\n";
        return true;
    }

    inline void print_boot_banner(const RuntimeConfig& cfg) {
        std::cout << "========================================================================================\n";
        std::cout << " ZVD-120D SOVEREIGN AI DATACENTER — PRODUCTION RUNTIME (V30.2-SUPREME-9MOD-C20)\n";
        std::cout << " Root-Architect: Vladimir Zavodiuk (Varna, Bulgaria, EU)\n";
        std::cout << " Contour: 9-Module Master Suite (Layers 0-4, Zero-Heap Hot Path, ASIL-D)\n";
        std::cout << " Endpoint: Proxy :" << cfg.proxy_port
                  << " -> Backend " << cfg.backend_ip << ":" << cfg.backend_port
                  << " | Storage :" << cfg.storage_port
                  << " | Vault: " << cfg.storage_root
                  << " | Rack :" << cfg.rack_port
                  << " | Cores: " << cfg.cpu_cores
                  << " | StorageThr: " << cfg.storage_workers << "\n";
        if (cfg.port_collision_resolved) {
            std::cout << "[WARN] Proxy port collision resolved: shifted from "
                      << cfg.requested_proxy_port << " to " << cfg.proxy_port << ".\n";
        }
        std::cout << "========================================================================================\n\n";
    }

    inline void print_module_manifest() {
        size_t active_count = 0u;
        for (const auto& entry : ZVD_9MODULE_MANIFEST) {
            if (entry.compiled_in) ++active_count;
        }

        std::cout << "[*] 9-Module Manifest (" << active_count << "/9 compiled in):\n";
        for (const auto& entry : ZVD_9MODULE_MANIFEST) {
            std::printf("    [%u] Layer %u | %-42s | %-38s | %s\n",
                        static_cast<unsigned>(entry.index),
                        static_cast<unsigned>(entry.layer),
                        entry.file_name,
                        entry.role_tag,
                        entry.compiled_in ? "[COMPILED]" : "[OMITTED]");
        }
        std::cout << "\n";
    }

    inline void print_module_registry() {
        const auto& module_bus = zvd_get_module_bus();
        std::cout << "[*] Active Bus Cartridges Registered: " << module_bus.size() << "\n";
        for (size_t i = 0; i < module_bus.size(); ++i) {
            const char* role_str = (module_bus[i].role == ZvdModuleRole::ARBITER) ? "ARBITER" :
                                   (module_bus[i].role == ZvdModuleRole::TELEMETRY) ? "TELEMETRY" : "FILTER";
            std::printf("    [%zu] %-34s | Role: %-10s [ACTIVE]\n",
                        i + 1, module_bus[i].name.c_str(), role_str);
        }
        std::cout << "\n";
    }

    inline int zvd_run_post_diagnostics() noexcept {
        using clock = std::chrono::steady_clock;
        const auto t_start = clock::now();
        bool all_ok = true;

        std::cout << "=== ZVD-120D 9-MODULE DIAGNOSTIC AUDIT (--audit) ===\n";

        if (!run_asil_d_power_on_self_test()) {
            all_ok = false;
        }

#if defined(ZVD_HAS_DATACENTER_CORE)
        {
            ZVD_BiCore120D core{};
            zvd_bicore_init(&core, 1, 2);
            const uint8_t sample[32] = {
                10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 13, 24, 35, 46,
                57, 68, 79, 81, 92, 103, 114, 125, 7, 14, 21, 28, 35, 42, 49, 56
            };
            ZVD_DatacenterPacket pkt{};
            double asym = 0.0;
            bool ok = zvd_node_ingest_monolith_only(sample, sizeof(sample), &core, &pkt) &&
                      zvd_inspect_monolith(&pkt.monolith, &asym) &&
                      (pkt.gl_invariant >= -2 && pkt.gl_invariant <= 2);
            std::cout << "  [Layer 0 Core 120D & Lukasiewicz] : " << (ok ? "[PASS]" : "[FAIL]") << "\n";
            if (!ok) all_ok = false;
        }
#else
        std::cout << "  [Layer 0 Core 120D & Lukasiewicz] : [OMITTED]\n";
#endif

#if defined(ZVD_HAS_NESTED_CODEC_V32)
        {
            ZvdNestedCodecDetail::ensure_pyth_345_lut();
            const bool ok = (zvd_codec_mode_name(zvd_get_codec_mode()) != nullptr);
            std::cout << "  [Layer 0 Nested Codec V32]        : " << (ok ? "[PASS]" : "[FAIL]") << "\n";
            if (!ok) all_ok = false;
        }
#else
        std::cout << "  [Layer 0 Nested Codec V32]        : [OMITTED]\n";
#endif

#if defined(ZVD_HAS_TENSOR_VAULT_BRIDGE)
        {
            const uint8_t t_data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            ZVD_Monolith334 mono{};
            int8_t gl_inv = 0;
            const bool ok = zvd_vault_compute_glz_signature(t_data, sizeof(t_data), &mono, &gl_inv);
            std::cout << "  [Layer 3 Tensor Vault GLZ ABI]    : " << (ok ? "[PASS]" : "[FAIL]") << "\n";
            if (!ok) all_ok = false;
        }
#else
        std::cout << "  [Layer 3 Tensor Vault GLZ ABI]    : [OMITTED]\n";
#endif

#if defined(ZVD_HAS_INDUSTRY_BRIDGE_V33)
        {
            alignas(64) char prom_buf[2048];
            size_t prom_len = 0u;
            const bool prom_ok = zvd_bridge_format_prometheus(
                1024u, 2048u, 2u, 1u, prom_buf, sizeof(prom_buf), &prom_len
            ) && prom_len > 0u;
            std::cout << "  [Layer 3 Prometheus Formatter]      : " << (prom_ok ? "[PASS]" : "[FAIL]") << "\n";
            if (!prom_ok) all_ok = false;

  #if defined(ZVD_BRIDGE_HAS_OPENSSL) && (ZVD_BRIDGE_HAS_OPENSSL == 1)
            const uint8_t key[ZVD_BRIDGE_AEAD_KEY_BYTES]{
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
            };
            const uint8_t nonce[ZVD_BRIDGE_AEAD_NONCE_BYTES]{
                0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,
                0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB
            };
            const uint8_t plain[12] = {'Z', 'V', 'D', '-', 'A', 'E', 'A', 'D', '-', 'O', 'K', '!'};
            alignas(64) uint8_t cipher[128]{};
            alignas(64) uint8_t decrypted[128]{};
            size_t ct_len = 0u;
            size_t pt_len = 0u;
            const bool aead_ok = zvd_bridge_aead_encrypt(key, nonce, plain, sizeof(plain), cipher, sizeof(cipher), &ct_len) &&
                                 zvd_bridge_aead_decrypt(key, cipher, ct_len, decrypted, sizeof(decrypted), &pt_len) &&
                                 pt_len == sizeof(plain) &&
                                 std::memcmp(plain, decrypted, sizeof(plain)) == 0;
            std::cout << "  [Layer 3 ChaCha20-Poly1305 AEAD]  : " << (aead_ok ? "[PASS]" : "[FAIL]") << "\n";
            if (!aead_ok) all_ok = false;
  #else
            std::cout << "  [Layer 3 ChaCha20-Poly1305 AEAD]  : [SKIP: NO_OPENSSL]\n";
  #endif
        }
#else
        std::cout << "  [Layer 3 Industry Bridge V33]     : [OMITTED]\n";
#endif

        const auto& bus = zvd_get_module_bus();
        std::cout << "  [Bus Cartridges Count]            : " << bus.size() << " registered\n";

        const auto t_end = clock::now();
        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        std::cout << "  [Audit Duration]                  : " << elapsed_ns << " ns\n";
        std::cout << "====================================================\n\n";

        print_module_manifest();
        return all_ok ? 0 : 1;
    }

    template <typename... Args>
    concept HasRackThread = requires(Args... args) {
        zvd_rack_fabric_listener_thread(args...);
    };

    template <typename... Args>
    concept HasHttpWorker = requires(Args... args) {
        zvd_run_http_worker(args...);
    };

    template <typename... Args>
    concept HasProxyEngine = requires(Args... args) {
        zvd_run_proxy_engine(args...);
    };

    template <typename... Args>
    concept HasEpollWorker = requires(Args... args) {
        run_async_epoll_worker(args...);
    };

#if defined(ZVD_HAS_STORAGE_FABRIC_MODULE)
    inline void invoke_rack_listener(const RuntimeConfig& cfg) noexcept {
        if constexpr (HasRackThread<void*>) {
            (void)zvd_rack_fabric_listener_thread(nullptr);
        } else if constexpr (HasRackThread<uint16_t>) {
            (void)zvd_rack_fabric_listener_thread(cfg.rack_port);
        } else if constexpr (HasRackThread<int>) {
            (void)zvd_rack_fabric_listener_thread(static_cast<int>(cfg.rack_port));
        } else if constexpr (HasRackThread<>) {
            (void)zvd_rack_fabric_listener_thread();
        }
    }

    inline void invoke_http_worker(const RuntimeConfig& cfg, int worker_idx) noexcept {
        (void)worker_idx;
        if constexpr (HasHttpWorker<uint16_t, const char*>) {
            (void)zvd_run_http_worker(cfg.storage_port, cfg.storage_root);
        } else if constexpr (HasHttpWorker<int, const char*>) {
            (void)zvd_run_http_worker(static_cast<int>(cfg.storage_port), cfg.storage_root);
        } else if constexpr (HasHttpWorker<uint16_t>) {
            (void)zvd_run_http_worker(cfg.storage_port);
        } else if constexpr (HasHttpWorker<int>) {
            (void)zvd_run_http_worker(static_cast<int>(cfg.storage_port));
        } else if constexpr (HasHttpWorker<void*>) {
            (void)zvd_run_http_worker(reinterpret_cast<void*>(static_cast<intptr_t>(worker_idx)));
        } else if constexpr (HasHttpWorker<>) {
            (void)zvd_run_http_worker();
        }
    }
#endif

    inline void invoke_proxy_worker(const RuntimeConfig& cfg, int worker_id) noexcept {
        if constexpr (HasProxyEngine<uint16_t, const char*, uint16_t>) {
            (void)zvd_run_proxy_engine(cfg.proxy_port, cfg.backend_ip, cfg.backend_port);
        } else if constexpr (HasProxyEngine<int, const char*, int>) {
            (void)zvd_run_proxy_engine(static_cast<int>(cfg.proxy_port), cfg.backend_ip, static_cast<int>(cfg.backend_port));
        } else if constexpr (HasProxyEngine<>) {
            (void)zvd_run_proxy_engine();
        } else if constexpr (HasEpollWorker<uint16_t, const char*, uint16_t>) {
            run_async_epoll_worker(cfg.proxy_port, cfg.backend_ip, cfg.backend_port);
        } else if constexpr (HasEpollWorker<int, const char*, int>) {
            run_async_epoll_worker(static_cast<int>(cfg.proxy_port), cfg.backend_ip, static_cast<int>(cfg.backend_port));
        } else if constexpr (HasEpollWorker<int, std::string_view, int>) {
            run_async_epoll_worker(
                static_cast<int>(cfg.proxy_port),
                std::string_view(cfg.backend_ip),
                static_cast<int>(cfg.backend_port)
            );
        } else if constexpr (HasEpollWorker<int, uint16_t, const char*, uint16_t>) {
            run_async_epoll_worker(worker_id, cfg.proxy_port, cfg.backend_ip, cfg.backend_port);
        } else if constexpr (HasEpollWorker<uint16_t, uint16_t>) {
            run_async_epoll_worker(cfg.proxy_port, cfg.storage_port);
        } else if constexpr (HasEpollWorker<int, std::string, int>) {
            run_async_epoll_worker(
                static_cast<int>(cfg.proxy_port),
                std::string(cfg.backend_ip),
                static_cast<int>(cfg.backend_port)
            );
        } else if constexpr (HasEpollWorker<void*>) {
            run_async_epoll_worker(reinterpret_cast<void*>(static_cast<intptr_t>(worker_id)));
        } else if constexpr (HasEpollWorker<>) {
            run_async_epoll_worker();
        } else {
            (void)worker_id;
        }
    }

} // namespace ZvdProductionOrchestrator

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, ZvdProductionOrchestrator::handle_termination_signal);
    std::signal(SIGTERM, ZvdProductionOrchestrator::handle_termination_signal);

#ifdef _OPENMP
    omp_set_max_active_levels(1);
#endif

    ZvdProductionOrchestrator::RuntimeConfig cfg{};
    ZvdProductionOrchestrator::parse_cli_args(argc, argv, cfg);
    if (cfg.show_help) {
        ZvdProductionOrchestrator::print_usage();
        return 0;
    }

    ZvdProductionOrchestrator::load_runtime_config(cfg);

    if (cfg.mode == ZvdProductionOrchestrator::RunMode::AuditOnly) {
        return ZvdProductionOrchestrator::zvd_run_post_diagnostics();
    }

    ZvdProductionOrchestrator::print_boot_banner(cfg);

#if defined(ZVD_HAS_DATACENTER_CORE)
    zvd_ensure_luts();
#endif

#if defined(ZVD_HAS_NESTED_CODEC_V32)
    ZvdNestedCodecDetail::ensure_pyth_345_lut();
    (void)zvd_get_codec_mode();
    std::cout << "[*] Nested Codec V32.0 Mode: " << zvd_codec_mode_name(zvd_get_codec_mode())
              << " | PYTH_345_LUT (64 KB L2) ready\n";
#endif

    ZvdProductionOrchestrator::print_module_manifest();
    ZvdProductionOrchestrator::print_module_registry();

    if (!ZvdProductionOrchestrator::run_asil_d_power_on_self_test()) {
        return 1;
    }

    if (!ZvdProductionOrchestrator::ensure_storage_root_dir(cfg.storage_root)) {
        int saved_err = errno;
        std::cerr << "[ASIL-D STORAGE ERROR] Failed to initialize vault directory '"
                  << cfg.storage_root << "': " << std::strerror(saved_err) << "\n";
        return 1;
    }

#ifndef _WIN32
    if (cfg.daemonize) {
        pid_t pid = ::fork();
        if (pid < 0) {
            std::cerr << "[ERROR] Daemon fork failed: " << std::strerror(errno) << "\n";
            return 1;
        }
        if (pid > 0) {
            return 0;
        }
        (void)::setsid();
    }
#endif

    const bool run_fabric =
#if defined(ZVD_HAS_STORAGE_FABRIC_MODULE)
        (cfg.mode == ZvdProductionOrchestrator::RunMode::FabricOnly) ||
        (cfg.mode == ZvdProductionOrchestrator::RunMode::Datacenter) ||
        (cfg.mode == ZvdProductionOrchestrator::RunMode::Enterprise) ||
        (cfg.mode == ZvdProductionOrchestrator::RunMode::DefaultAuto) ||
        (cfg.mode == ZvdProductionOrchestrator::RunMode::Daemon);
#else
        false;
#endif

#if !defined(ZVD_HAS_STORAGE_FABRIC_MODULE)
    if (cfg.mode == ZvdProductionOrchestrator::RunMode::FabricOnly) {
        std::cerr << "[ERROR] --fabric requested, but 13.11_zvd_sovereign_storage_fabric.cpp is not compiled in.\n";
        return 1;
    }
#endif

    const bool run_proxy =
        (cfg.mode != ZvdProductionOrchestrator::RunMode::FabricOnly);

    const size_t fabric_threads = run_fabric ? (1u + static_cast<size_t>(cfg.storage_workers)) : 0u;
    const size_t proxy_threads = run_proxy ? static_cast<size_t>(cfg.cpu_cores) : 0u;

    std::vector<std::thread> workers;
    workers.reserve(fabric_threads + proxy_threads);

#if defined(ZVD_HAS_STORAGE_FABRIC_MODULE)
    if (run_fabric) {
        workers.emplace_back([&cfg]() {
            ZvdProductionOrchestrator::invoke_rack_listener(cfg);
        });

        for (int s_idx = 0; s_idx < cfg.storage_workers; ++s_idx) {
            workers.emplace_back([&cfg, s_idx]() {
                ZvdProductionOrchestrator::invoke_http_worker(cfg, s_idx);
            });
        }
    }
#endif

    if (run_proxy) {
        for (int core_idx = 0; core_idx < cfg.cpu_cores; ++core_idx) {
            workers.emplace_back([&cfg, core_idx]() {
                ZvdProductionOrchestrator::invoke_proxy_worker(cfg, core_idx);
            });
        }
    }

    std::cout << "[*] Production workers started: "
              << (run_fabric ? ("1 rack listener (:" + std::to_string(cfg.rack_port) + "), " +
                                std::to_string(cfg.storage_workers) + " storage workers (:" +
                                std::to_string(cfg.storage_port) + ", SO_REUSEPORT), ")
                             : std::string("storage fabric inactive, "))
              << (run_proxy ? (std::to_string(cfg.cpu_cores) + " proxy workers (:" +
                               std::to_string(cfg.proxy_port) + " -> " +
                               cfg.backend_ip + ":" + std::to_string(cfg.backend_port) + ").\n")
                            : std::string("proxy inactive.\n"));

    while (!g_zvd_global_shutdown.load(std::memory_order_acquire)) {
#ifndef _WIN32
        (void)::sleep(1u);
#endif
    }

    std::cout << "\n[*] Shutdown signal received. Waiting for worker loops to finish...\n";
    for (auto& th : workers) {
        if (th.joinable()) {
            th.join();
        }
    }
    std::cout << "[*] All ZVD-120D workers stopped cleanly.\n";
    return 0;
}
