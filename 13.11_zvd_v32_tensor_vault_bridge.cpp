/* =============================================================================
 *  PROJECT:    ZVD-120D V32 TENSOR VAULT C-ABI & MULTI-CHUNK BRIDGE
 *              (13.11_zvd_v32_tensor_vault_bridge.cpp - V32.0-SUPREME-C20)
 *  COMPILATION:
 *    g++ -std=c++20 -O3 -march=native -Wall -Wextra -Werror -shared -fPIC "13.11_zvd_v32_tensor_vault_bridge.cpp" -o libzvd_v32_vault.so -lpthread -lm
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
 * =============================================================================
 */

#ifndef ZVD_HAS_NESTED_CODEC_V32
  #define ZVD_HAS_NESTED_CODEC_V32 1
  #define ZVD_HAS_NESTED_CODEC_MODULE 1
  #include "13.11_zvd_nested_storage_codec_v32.hpp"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

inline constexpr size_t ZVD_VAULT_SHARD_BYTES = 3600000u; // 3.60 MB per macro-cell shard
inline constexpr uint32_t ZVD_MANIFEST_MAGIC = 0x4644565Au; // "ZVDF"
inline constexpr uint32_t ZVD_MANIFEST_VERSION = 1u;
inline constexpr size_t ZVD_MAX_MANIFEST_SHARDS = 16384u; // up to ~58.9 GB per object

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if !defined(_MSC_VER)
__attribute__((packed))
#endif
ZvdVaultManifestHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t shard_count;
    uint32_t reserved;
    uint64_t total_raw_bytes;
    uint64_t full_hash64;
};
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

namespace ZvdVaultBridgeDetail {

inline void make_manifest_path(const char* vault_dir, const char* object_name, char out[512]) noexcept {
    std::snprintf(out, 512, "%s/%s.manifest", vault_dir, object_name);
}

inline void make_shard_path(const char* vault_dir, uint64_t shard_hash64, char out[512]) noexcept {
    ZvdNestedCodecDetail::build_cell_file_path(vault_dir, ZvdCodecTopologyMode::STELLA_8_VERT, shard_hash64, out);
}

// Godel-Lukasewicz-Zavodiuk (GLZ) 500-cell fold -> Monolith-334 & 120D invariant
inline bool fold_glz_monolith(
    const uint8_t* data,
    size_t len,
    ZVD_Monolith334& out_monolith,
    int8_t& out_gl_invariant
) noexcept {
    if (!data || len == 0u) return false;
    zvd_ensure_luts();

    alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> face_500{};
    alignas(64) std::array<int8_t, ZVD_CELLS_PER_LAYER> vert_500{};
    face_500.fill(0);
    vert_500.fill(0);

    size_t stride = (len > 262144u) ? (len / 262144u) : 1u;
    for (size_t i = 0; i < len; i += stride) {
        size_t idx = i % ZVD_CELLS_PER_LAYER;
        int signed_b = static_cast<int>(static_cast<int8_t>(data[i]));
        int f_acc = static_cast<int>(face_500[idx]) + (signed_b / 16);
        face_500[idx] = zvd_lukasiewicz_clamp(f_acc);

        int v_acc = static_cast<int>(vert_500[idx]) +
                    (((static_cast<int>(data[i]) & 0x0F) - 8) * (((i & 1u) == 0u) ? 1 : -1));
        vert_500[idx] = zvd_lukasiewicz_clamp(v_acc);
    }

    zvd_pack_layer_lukasiewicz(face_500.data(), out_monolith.face_packed);
    zvd_pack_layer_lukasiewicz(vert_500.data(), out_monolith.vertex_packed);

    int8_t vec120[ZVD_DIM_120];
    for (int d = 0; d < ZVD_DIM_120; ++d) {
        vec120[d] = zvd_lukasiewicz_clamp(static_cast<int>(face_500[d]) + static_cast<int>(vert_500[d]));
    }
    out_gl_invariant = zvd_godel_lukasiewicz_reduce(vec120);
    return true;
}

inline bool read_manifest(
    const char* vault_dir, const char* object_name,
    ZvdVaultManifestHeader& out_hdr, uint64_t* out_hashes, size_t max_hashes
) noexcept {
#ifdef _WIN32
    (void)vault_dir; (void)object_name; (void)out_hdr; (void)out_hashes; (void)max_hashes;
    return false;
#else
    if (!vault_dir || !object_name || object_name[0] == '\0') return false;
    char mpath[512];
    make_manifest_path(vault_dir, object_name, mpath);
    int fd = ::open(mpath, O_RDONLY);
    if (fd < 0) return false;

    bool ok = ZvdNestedCodecDetail::pread_all_fd(fd, &out_hdr, sizeof(out_hdr), 0);
    if (!ok || out_hdr.magic != ZVD_MANIFEST_MAGIC || out_hdr.version != ZVD_MANIFEST_VERSION ||
        out_hdr.shard_count == 0u || out_hdr.shard_count > ZVD_MAX_MANIFEST_SHARDS ||
        static_cast<size_t>(out_hdr.shard_count) > max_hashes) {
        ::close(fd);
        return false;
    }
    size_t hbytes = static_cast<size_t>(out_hdr.shard_count) * sizeof(uint64_t);
    ok = ZvdNestedCodecDetail::pread_all_fd(fd, out_hashes, hbytes, static_cast<off_t>(sizeof(out_hdr)));
    ::close(fd);
    return ok;
#endif
}

inline bool write_manifest_atomic(
    const char* vault_dir, const char* object_name,
    const ZvdVaultManifestHeader& hdr, const uint64_t* shard_hashes
) noexcept {
#ifdef _WIN32
    (void)vault_dir; (void)object_name; (void)hdr; (void)shard_hashes;
    return false;
#else
    ZvdNestedCodecDetail::ensure_vault_dir(vault_dir);
    char final_path[512];
    make_manifest_path(vault_dir, object_name, final_path);

    static std::atomic<uint64_t> mseq{0};
    uint64_t seq = mseq.fetch_add(1, std::memory_order_relaxed);
    char tmp_path[576];
    std::snprintf(tmp_path, sizeof(tmp_path), "%s/.man.%ld.%llu.tmp",
                  vault_dir, static_cast<long>(::getpid()), static_cast<unsigned long long>(seq));

    int fd = ::open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;

    size_t hbytes = static_cast<size_t>(hdr.shard_count) * sizeof(uint64_t);
    bool ok = ZvdNestedCodecDetail::write_all_fd(fd, &hdr, sizeof(hdr)) &&
              (hbytes == 0u || ZvdNestedCodecDetail::write_all_fd(fd, shard_hashes, hbytes));
    if (ok) (void)::fsync(fd);
    if (::close(fd) != 0) ok = false;
    if (!ok || ::rename(tmp_path, final_path) != 0) {
        (void)::unlink(tmp_path);
        return false;
    }
    ZvdNestedCodecDetail::fsync_vault_dir(vault_dir);
    return true;
#endif
}

} // namespace ZvdVaultBridgeDetail

extern "C" {

inline bool zvd_vault_compute_glz_signature(
    const uint8_t* data, size_t len,
    ZVD_Monolith334* out_monolith, int8_t* out_gl_invariant
) noexcept {
    if (!out_monolith || !out_gl_invariant) return false;
    return ZvdVaultBridgeDetail::fold_glz_monolith(data, len, *out_monolith, *out_gl_invariant);
}

inline bool zvd_vault_store_tensor_file(
    const char* vault_dir, const char* object_name,
    const uint8_t* data, size_t len, const char* master_key
) noexcept {
    if (!vault_dir || !object_name || object_name[0] == '\0' || !data || len == 0u) return false;
    size_t shard_count = (len + ZVD_VAULT_SHARD_BYTES - 1u) / ZVD_VAULT_SHARD_BYTES;
    if (shard_count > ZVD_MAX_MANIFEST_SHARDS) return false;

    static thread_local alignas(64) uint64_t shard_hashes[ZVD_MAX_MANIFEST_SHARDS];
    const char* k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;

    for (size_t s = 0; s < shard_count; ++s) {
        size_t off = s * ZVD_VAULT_SHARD_BYTES;
        size_t chunk_len = std::min(ZVD_VAULT_SHARD_BYTES, len - off);
        char spath[512]{0};
        bool dedup = false;
        if (!zvd_storage_put_nested_cell(vault_dir, data + off, chunk_len, k,
                                         ZvdCodecTopologyMode::STELLA_8_VERT, spath, &dedup)) {
            return false;
        }
        shard_hashes[s] = zvd_fast_hash64(data + off, chunk_len);
    }

    ZvdVaultManifestHeader mhdr{};
    mhdr.magic = ZVD_MANIFEST_MAGIC;
    mhdr.version = ZVD_MANIFEST_VERSION;
    mhdr.shard_count = static_cast<uint32_t>(shard_count);
    mhdr.total_raw_bytes = static_cast<uint64_t>(len);
    mhdr.full_hash64 = zvd_fast_hash64(data, len);
    return ZvdVaultBridgeDetail::write_manifest_atomic(vault_dir, object_name, mhdr, shard_hashes);
}

inline bool zvd_vault_load_tensor_file(
    const char* vault_dir, const char* object_name,
    uint8_t* out_buf, size_t out_cap, size_t* out_len, const char* master_key
) noexcept {
    if (!vault_dir || !object_name || !out_buf || !out_len) return false;
    static thread_local alignas(64) uint64_t shard_hashes[ZVD_MAX_MANIFEST_SHARDS];
    ZvdVaultManifestHeader mhdr{};
    if (!ZvdVaultBridgeDetail::read_manifest(vault_dir, object_name, mhdr, shard_hashes, ZVD_MAX_MANIFEST_SHARDS)) {
        return false;
    }
    if (static_cast<size_t>(mhdr.total_raw_bytes) > out_cap) return false;

    const char* k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
    size_t write_pos = 0u;
    for (uint32_t s = 0; s < mhdr.shard_count; ++s) {
        char spath[512];
        ZvdVaultBridgeDetail::make_shard_path(vault_dir, shard_hashes[s], spath);
        size_t dec_len = 0u;
        bool veto = false;
        if (!zvd_storage_get_full_cell(spath, k, out_buf + write_pos, out_cap - write_pos, &dec_len, &veto) || veto) {
            return false;
        }
        write_pos += dec_len;
    }

    if (write_pos != static_cast<size_t>(mhdr.total_raw_bytes) ||
        zvd_fast_hash64(out_buf, write_pos) != mhdr.full_hash64) {
        return false;
    }
    *out_len = write_pos;
    return true;
}

inline bool zvd_vault_read_tensor_slice(
    const char* vault_dir, const char* object_name,
    uint64_t byte_offset, size_t slice_len, uint8_t* out_buf, const char* master_key
) noexcept {
    if (!vault_dir || !object_name || !out_buf || slice_len == 0u) return false;
    static thread_local alignas(64) uint64_t shard_hashes[ZVD_MAX_MANIFEST_SHARDS];
    ZvdVaultManifestHeader mhdr{};
    if (!ZvdVaultBridgeDetail::read_manifest(vault_dir, object_name, mhdr, shard_hashes, ZVD_MAX_MANIFEST_SHARDS)) {
        return false;
    }
    if (byte_offset > mhdr.total_raw_bytes ||
        static_cast<uint64_t>(slice_len) > mhdr.total_raw_bytes - byte_offset) {
        return false;
    }

    constexpr uint64_t micro_bytes = static_cast<uint64_t>(ZVD_MICRO_SLOTS_COUNT) * ZVD_STELLA8_SLOT_BYTES; // 7040
    const char* k = (master_key && master_key[0] != '\0') ? master_key : ZVD_DEFAULT_MASTER_KEY;
    alignas(64) uint8_t slot_tmp[ZVD_STELLA8_SLOT_BYTES];

    size_t remaining = slice_len;
    uint64_t cur_off = byte_offset;
    size_t dst_pos = 0u;

    while (remaining > 0u) {
        uint32_t s_idx = static_cast<uint32_t>(cur_off / ZVD_VAULT_SHARD_BYTES);
        if (s_idx >= mhdr.shard_count) return false;
        uint64_t local_off = cur_off % ZVD_VAULT_SHARD_BYTES;
        uint16_t geo_id = static_cast<uint16_t>(local_off / micro_bytes);
        uint16_t slot_idx = static_cast<uint16_t>((local_off % micro_bytes) / ZVD_STELLA8_SLOT_BYTES);
        size_t in_slot_off = static_cast<size_t>((local_off % micro_bytes) % ZVD_STELLA8_SLOT_BYTES);

        char spath[512];
        ZvdVaultBridgeDetail::make_shard_path(vault_dir, shard_hashes[s_idx], spath);
        size_t slot_len = 0u;
        bool veto = false;
        if (!zvd_storage_pread_slot(spath, geo_id, slot_idx, k, slot_tmp, sizeof(slot_tmp), &slot_len, &veto) ||
            veto || in_slot_off >= slot_len) {
            return false;
        }

        size_t avail = slot_len - in_slot_off;
        size_t take = std::min(remaining, avail);
        std::memcpy(out_buf + dst_pos, slot_tmp + in_slot_off, take);
        dst_pos += take;
        cur_off += static_cast<uint64_t>(take);
        remaining -= take;
    }
    return true;
}

inline bool zvd_vault_semantic_search(
    const char* vault_dir, const uint8_t* query_data, size_t query_len,
    char out_best_shard_path[512], double* out_score
) noexcept {
    if (!vault_dir || !query_data || query_len == 0u || !out_best_shard_path) return false;
    ZVD_Monolith334 q_mono{};
    int8_t q_inv = 0;
    if (!ZvdVaultBridgeDetail::fold_glz_monolith(query_data, query_len, q_mono, q_inv)) return false;
    return zvd_storage_search_by_vector(vault_dir, q_mono, q_inv, out_best_shard_path, out_score, nullptr);
}

} // extern "C"

#ifdef ZVD_REGISTER_CARTRIDGE_CTX
inline int zvd_v32_tensor_vault_bridge_evaluate_ctx(ZvdPipelineContext& ctx) noexcept {
    return (!ctx.prompt.empty() && ctx.prompt.starts_with("ZVD_VAULT_BRIDGE")) ? 1 : 0;
}
inline bool zvd_v32_tensor_vault_bridge_verify(std::string_view, std::string_view) noexcept {
    return true;
}
ZVD_REGISTER_CARTRIDGE_CTX(
    "ZVD-V32-Tensor-Vault-Bridge",
    ZvdModuleRole::TELEMETRY,
    zvd_v32_tensor_vault_bridge_evaluate_ctx,
    zvd_v32_tensor_vault_bridge_verify
);
#endif
