#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
UNIVERSAL DATACENTER NODE V24.1 — INTER-DC
Быстрое копирование → защищённое хранение → передача в другой дата-центр
================================================================================

Назначение:
  Узел, внутри которого данные:
    1. Быстро копируются (zero-copy / memoryview / numpy share)
    2. Надёжно хранятся (BXOS + AES-256-GCM, троичная санитизация)
    3. Передаются в другой ЦОД (Optical Transport + handoff)

Стек:
  • FastBuffer — zero-copy layer
  • Numba L3 triage (троичный: -1 / 0 / 1) + BXOS guard
  • BXOS + AES-GCM хранилище (устойчивое к FS)
  • Optical Inter-DC (CD / Kerr / OSNR / distance → GO / REVISE / NO_GO)
  • Пайплайн replicate: copy → store → transfer

Автор: Владимир Заводюк (Vladimir Zavodiuk)
ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
VERSION: 24.1-INTERDC
GitHub: https://github.com/Zavodiuk
================================================================================
"""

from __future__ import annotations

import os
import sys
import time
import json
import zlib
import struct
import hashlib
import base64
from typing import Any, Dict, List, Optional, Tuple, Literal, Union
from dataclasses import dataclass, field
from collections import deque
from enum import Enum

import numpy as np

# ---------------------------------------------------------------------------
# Optional deps
# ---------------------------------------------------------------------------
try:
    from numba import njit, prange
    HAS_NUMBA = True
except ImportError:
    HAS_NUMBA = False

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    HAS_AESGCM = True
except ImportError:
    HAS_AESGCM = False

try:
    from fastapi import FastAPI, HTTPException
    from pydantic import BaseModel
    import uvicorn
    HAS_FASTAPI = True
except ImportError:
    HAS_FASTAPI = False


# ==============================================================================
# 1. FAST COPY LAYER
# ==============================================================================

class FastBuffer:
    """
    Быстрый буфер с приоритетом zero-copy.
    view()  — shared storage
    clone() — true ownership copy
    """
    __slots__ = ("_data", "_owned", "_meta")

    def __init__(
        self,
        data: Union[bytes, bytearray, memoryview, np.ndarray],
        owned: bool = False,
        meta: Optional[Dict] = None,
    ) -> None:
        if isinstance(data, np.ndarray):
            self._data = data
        elif isinstance(data, (bytes, bytearray)):
            self._data = memoryview(data)
        else:
            self._data = data
        self._owned = owned
        self._meta = meta or {}

    @property
    def size(self) -> int:
        if isinstance(self._data, np.ndarray):
            return int(self._data.nbytes)
        return len(self._data)

    def as_bytes(self) -> bytes:
        if isinstance(self._data, np.ndarray):
            return self._data.tobytes()
        return bytes(self._data)

    def as_memoryview(self) -> memoryview:
        if isinstance(self._data, np.ndarray):
            return memoryview(self._data)
        if isinstance(self._data, memoryview):
            return self._data
        return memoryview(self._data)

    def view(self) -> "FastBuffer":
        return FastBuffer(self._data, owned=False, meta=dict(self._meta))

    def clone(self) -> "FastBuffer":
        if isinstance(self._data, np.ndarray):
            return FastBuffer(self._data.copy(), owned=True, meta=dict(self._meta))
        return FastBuffer(bytes(self._data), owned=True, meta=dict(self._meta))

    def checksum(self) -> str:
        """
        Быстрый отпечаток: первые 4 KB + последние 4 KB + размер.
        Избегает полной аллокации на очень больших буферах.
        """
        raw = self.as_bytes()
        n = len(raw)
        if n <= 8192:
            return hashlib.sha256(raw).hexdigest()[:16]
        h = hashlib.sha256()
        h.update(raw[:4096])
        h.update(raw[-4096:])
        h.update(struct.pack("<Q", n))
        return h.hexdigest()[:16]


def fast_copy(
    src: Union[bytes, bytearray, memoryview, np.ndarray, FastBuffer],
) -> FastBuffer:
    if isinstance(src, FastBuffer):
        return src.clone()
    if isinstance(src, np.ndarray):
        return FastBuffer(src.copy(), owned=True)
    return FastBuffer(bytes(src), owned=True)


# ==============================================================================
# 2. NUMBA HOT PATH — троичная симметрия [-1, 0, 1]
# ==============================================================================

if HAS_NUMBA:
    @njit(cache=True, fastmath=True)
    def _bxos_guard(signal: np.ndarray) -> np.ndarray:
        """Жёсткое приведение к тритам: -1, 0, 1."""
        out = np.empty(signal.shape, dtype=np.int8)
        flat_in = signal.ravel()
        flat_out = out.ravel()
        for i in range(flat_in.size):
            v = flat_in[i]
            if v > 1:
                v = 1
            elif v < -1:
                v = -1
            flat_out[i] = np.int8(v)
        return out

    @njit(cache=True, parallel=True, fastmath=True)
    def _l3_triage(
        scores: np.ndarray, low: float = 0.33, high: float = 0.66
    ) -> np.ndarray:
        """-1 = REJECT, 0 = CLARIFY, 1 = OK."""
        n = scores.shape[0]
        out = np.empty(n, dtype=np.int8)
        for i in prange(n):
            m = 0.0
            for j in range(scores.shape[1]):
                m += scores[i, j]
            m /= scores.shape[1]
            if m < low:
                out[i] = -1
            elif m > high:
                out[i] = 1
            else:
                out[i] = 0
        return out
else:
    def _bxos_guard(signal: np.ndarray) -> np.ndarray:
        return np.clip(signal, -1, 1).astype(np.int8)

    def _l3_triage(
        scores: np.ndarray, low: float = 0.33, high: float = 0.66
    ) -> np.ndarray:
        m = scores.mean(axis=1)
        out = np.zeros(len(m), dtype=np.int8)
        out[m < low] = -1
        out[m > high] = 1
        return out


# ==============================================================================
# 3. BXOS + AES-GCM STORAGE (устойчивое)
# ==============================================================================

class BXOSCrypto:
    def __init__(self, storage_dir: str) -> None:
        self.key_path = os.path.join(storage_dir, "_bxos.key")
        self.enabled = HAS_AESGCM
        self._aes: Optional[Any] = None
        if self.enabled:
            key = self._load_or_create()
            self._aes = AESGCM(key)

    def _load_or_create(self) -> bytes:
        if os.path.exists(self.key_path):
            with open(self.key_path, "rb") as f:
                return f.read()
        key = AESGCM.generate_key(bit_length=256)
        os.makedirs(os.path.dirname(self.key_path) or ".", exist_ok=True)
        with open(self.key_path, "wb") as f:
            f.write(key)
        return key

    def encrypt(self, plain: bytes) -> bytes:
        if not self.enabled or self._aes is None:
            return plain
        nonce = os.urandom(12)
        return nonce + self._aes.encrypt(nonce, plain, None)

    def decrypt(self, blob: bytes) -> bytes:
        if not self.enabled or self._aes is None:
            return blob
        return self._aes.decrypt(blob[:12], blob[12:], None)


@dataclass
class BXOSRecord:
    record_id: str
    data_type: str
    payload: bytes
    original_size: int
    checksum: str
    ontology: List[int] = field(default_factory=lambda: [0, 0, 0, 0, 0])
    ts_ns: int = field(default_factory=time.time_ns)

    def to_wire(self) -> bytes:
        header = {
            "id": self.record_id,
            "type": self.data_type,
            "orig": self.original_size,
            "sum": self.checksum,
            "ont": self.ontology,
            "ts": self.ts_ns,
        }
        h = json.dumps(header, separators=(",", ":")).encode("utf-8")
        return struct.pack("<I", len(h)) + h + self.payload

    @classmethod
    def from_wire(cls, raw: bytes) -> "BXOSRecord":
        hlen = struct.unpack("<I", raw[:4])[0]
        header = json.loads(raw[4 : 4 + hlen].decode("utf-8"))
        return cls(
            record_id=header["id"],
            data_type=header["type"],
            payload=raw[4 + hlen :],
            original_size=header["orig"],
            checksum=header["sum"],
            ontology=header.get("ont", [0, 0, 0, 0, 0]),
            ts_ns=header.get("ts", 0),
        )


class BXOSStore:
    """
    Защищённое хранилище.
    - compress → encrypt → disk
    - registry персистентный + in-memory fallback при ошибке FS
    """

    def __init__(self, storage_dir: str = "./dc_v24_1_storage") -> None:
        self.dir = storage_dir
        os.makedirs(self.dir, exist_ok=True)
        self.crypto = BXOSCrypto(self.dir)
        self.registry: Dict[str, Dict[str, Any]] = {}
        self._load_registry()

    def _reg_path(self) -> str:
        return os.path.join(self.dir, "_registry.json")

    def _load_registry(self) -> None:
        p = self._reg_path()
        if os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    self.registry = json.load(f)
            except Exception:
                self.registry = {}

    def _save_registry(self) -> None:
        try:
            with open(self._reg_path(), "w", encoding="utf-8") as f:
                json.dump(self.registry, f, ensure_ascii=False, indent=1)
        except OSError:
            pass  # keep in-memory

    def _path_for(self, record_id: str) -> str:
        h = hashlib.sha256(record_id.encode()).hexdigest()[:32]
        return os.path.join(self.dir, f"{h}.bxos")

    def put(
        self,
        record_id: str,
        data: Union[bytes, str, dict, list, np.ndarray],
        ontology: Optional[List[int]] = None,
    ) -> str:
        if isinstance(data, np.ndarray):
            dtype = "tensor"
            sanitized = _bxos_guard(data)  # троичная санитизация
            hdr = json.dumps(
                {"shape": sanitized.shape, "dtype": str(sanitized.dtype)}
            ).encode()
            raw = struct.pack("<I", len(hdr)) + hdr + sanitized.tobytes()
        elif isinstance(data, str):
            dtype, raw = "text", data.encode("utf-8")
        elif isinstance(data, (dict, list)):
            dtype, raw = "json", json.dumps(data, ensure_ascii=False).encode("utf-8")
        else:
            dtype, raw = "bin", bytes(data)

        compressed = zlib.compress(raw, 3)  # level 3 — баланс скорость/размер
        checksum = hashlib.sha256(compressed).hexdigest()
        rec = BXOSRecord(
            record_id=record_id,
            data_type=dtype,
            payload=compressed,
            original_size=len(raw),
            checksum=checksum,
            ontology=ontology or [0, 0, 0, 0, 0],
        )
        wire = rec.to_wire()
        encrypted = self.crypto.encrypt(wire)

        path = self._path_for(record_id)
        written = False
        try:
            with open(path, "wb") as f:
                f.write(encrypted)
            written = True
        except OSError:
            path = ""

        self.registry[record_id] = {
            "path": path,
            "type": dtype,
            "orig": len(raw),
            "comp": len(compressed),
            "enc": len(encrypted),
            "sum": checksum,
            "ts": rec.ts_ns,
            "wire": None if written else encrypted,  # in-mem fallback
        }
        self._save_registry()
        return path or f"mem://{record_id}"

    def get(self, record_id: str) -> Any:
        if record_id not in self.registry:
            raise KeyError(f"record not found: {record_id}")
        meta = self.registry[record_id]
        path = meta.get("path") or ""
        if path and os.path.exists(path):
            with open(path, "rb") as f:
                encrypted = f.read()
        else:
            encrypted = meta.get("wire")
            if encrypted is None:
                raise KeyError(f"record data missing: {record_id}")

        wire = self.crypto.decrypt(encrypted)
        rec = BXOSRecord.from_wire(wire)
        if hashlib.sha256(rec.payload).hexdigest() != rec.checksum:
            raise ValueError(f"integrity fail: {record_id}")
        raw = zlib.decompress(rec.payload)

        if rec.data_type == "tensor":
            hlen = struct.unpack("<I", raw[:4])[0]
            hdr = json.loads(raw[4 : 4 + hlen].decode())
            return np.frombuffer(
                raw[4 + hlen :], dtype=hdr["dtype"]
            ).reshape(hdr["shape"])
        if rec.data_type == "json":
            return json.loads(raw.decode("utf-8"))
        if rec.data_type == "text":
            return raw.decode("utf-8")
        return raw

    def exists(self, record_id: str) -> bool:
        return record_id in self.registry

    def list_ids(self) -> List[str]:
        return list(self.registry.keys())
# ==============================================================================
# 4. OPTICAL / INTER-DC TRANSPORT
# ==============================================================================

class FiberDecision(str, Enum):
    GO = "GO"
    REVISE = "REVISE"
    NO_GO = "NO_GO"


@dataclass
class TransferResult:
    success: bool
    decision: str
    bytes_sent: int
    latency_ms: float
    target_dc: str
    message: str


@dataclass
class PipelineStats:
    stage: str
    latency_ms: float
    bytes_in: int = 0
    bytes_out: int = 0


@dataclass
class InterDCResult:
    record_id: str
    copied: bool
    stored: bool
    transferred: bool
    target_dc: str
    total_latency_ms: float
    stages: List[PipelineStats]
    optical_decision: str
    checksum: str


class OpticalInterDC:
    """
    Оптический транспорт + handoff в peer ЦОД.
    Stability index: OSNR − CD − Kerr − distance penalty.
    """

    def __init__(self, local_dc_id: str = "DC-A") -> None:
        self.local_dc_id = local_dc_id
        self._peer_buffers: Dict[str, deque] = {}

    def _stability(
        self, distance_km: float, cd: float, kerr: float, osnr: float
    ) -> float:
        dist_pen = min(0.25, distance_km / 2000.0)
        return (osnr / 30.0) - (cd * 0.4 + kerr * 0.4) - dist_pen

    def evaluate_link(
        self,
        distance_km: float = 120.0,
        chromatic_dispersion: float = 0.04,
        kerr_nonlinearity: float = 0.015,
        osnr_db: float = 26.0,
    ) -> FiberDecision:
        s = self._stability(
            distance_km, chromatic_dispersion, kerr_nonlinearity, osnr_db
        )
        if s > 0.55:
            return FiberDecision.GO
        if s > 0.28:
            return FiberDecision.REVISE
        return FiberDecision.NO_GO

    def transfer(
        self,
        payload: bytes,
        target_dc: str,
        distance_km: float = 120.0,
        chromatic_dispersion: float = 0.04,
        kerr_nonlinearity: float = 0.015,
        osnr_db: float = 26.0,
        force: bool = False,
    ) -> TransferResult:
        t0 = time.perf_counter()
        decision = self.evaluate_link(
            distance_km, chromatic_dispersion, kerr_nonlinearity, osnr_db
        )
        if decision == FiberDecision.NO_GO and not force:
            return TransferResult(
                success=False,
                decision=decision.value,
                bytes_sent=0,
                latency_ms=(time.perf_counter() - t0) * 1000,
                target_dc=target_dc,
                message="Link NO_GO — transfer aborted",
            )
        if target_dc not in self._peer_buffers:
            self._peer_buffers[target_dc] = deque(maxlen=2048)
        self._peer_buffers[target_dc].append(
            {
                "from": self.local_dc_id,
                "payload": payload,
                "ts": time.time_ns(),
                "size": len(payload),
                "decision": decision.value,
            }
        )
        time.sleep(min(0.0015, len(payload) / 80_000_000))
        return TransferResult(
            success=True,
            decision=decision.value,
            bytes_sent=len(payload),
            latency_ms=(time.perf_counter() - t0) * 1000,
            target_dc=target_dc,
            message=f"Handoff OK → {target_dc} ({decision.value})",
        )

    def receive_from(self, peer_dc: str) -> Optional[bytes]:
        q = self._peer_buffers.get(peer_dc)
        if not q:
            return None
        return q.popleft()["payload"]


# ==============================================================================
# 5. MAIN NODE — COPY → STORE → TRANSFER
# ==============================================================================

class UniversalDatacenterNodeV24:
    """
    Главный узел V24.1 Inter-DC.

    Типичный пайплайн:
      copy_in   → FastBuffer
      store     → BXOS + AES-GCM (троичная санитизация тензоров)
      transfer  → Optical handoff
      replicate → copy + store + transfer одной командой
    """

    def __init__(
        self,
        dc_id: str = "DC-A",
        storage_dir: str = "./dc_v24_1_storage",
    ) -> None:
        self.dc_id = dc_id
        self._storage = BXOSStore(storage_dir)
        self.optical = OpticalInterDC(local_dc_id=dc_id)
        self._warmup()

    def _warmup(self) -> None:
        dummy = np.random.rand(8, 4).astype(np.float64)
        _ = _l3_triage(dummy)
        mat = np.random.randint(-2, 3, (8, 8)).astype(np.int8)
        _ = _bxos_guard(mat)

    def copy_in(
        self, data: Union[bytes, bytearray, memoryview, np.ndarray, FastBuffer]
    ) -> FastBuffer:
        return fast_copy(data)

    def store(
        self,
        record_id: str,
        data: Union[bytes, str, dict, list, np.ndarray, FastBuffer],
        ontology: Optional[List[int]] = None,
    ) -> str:
        # FastBuffer с ndarray внутри → сохраняем tensor-путь (ternary guard)
        if isinstance(data, FastBuffer):
            if isinstance(data._data, np.ndarray):
                payload: Any = data._data
            else:
                payload = data.as_bytes()
        else:
            payload = data
        return self._storage.put(record_id, payload, ontology=ontology)

    def load(self, record_id: str) -> Any:
        return self._storage.get(record_id)

    def exists(self, record_id: str) -> bool:
        return self._storage.exists(record_id)

    def transfer_to(
        self,
        payload: Union[bytes, FastBuffer],
        target_dc: str,
        distance_km: float = 120.0,
        **link_kwargs: Any,
    ) -> TransferResult:
        raw = payload.as_bytes() if isinstance(payload, FastBuffer) else payload
        return self.optical.transfer(
            raw, target_dc, distance_km=distance_km, **link_kwargs
        )

    def replicate(
        self,
        record_id: str,
        data: Union[bytes, str, dict, list, np.ndarray],
        target_dc: str,
        ontology: Optional[List[int]] = None,
        distance_km: float = 120.0,
        store_locally: bool = True,
        **link_kwargs: Any,
    ) -> InterDCResult:
        t0 = time.perf_counter()
        stages: List[PipelineStats] = []

        # 1. Copy
        t1 = time.perf_counter()
        if isinstance(data, (str, dict, list)):
            raw_for_copy: bytes = (
                data.encode("utf-8")
                if isinstance(data, str)
                else json.dumps(data, ensure_ascii=False).encode("utf-8")
            )
            buf = self.copy_in(raw_for_copy)
        else:
            buf = self.copy_in(data)
        stages.append(
            PipelineStats(
                "copy",
                (time.perf_counter() - t1) * 1000,
                bytes_in=buf.size,
                bytes_out=buf.size,
            )
        )

        # 2. Store — тип сохраняем для ndarray/str/dict/list, иначе берём копию
        stored = False
        if store_locally:
            t2 = time.perf_counter()
            if isinstance(data, (np.ndarray, str, dict, list)):
                self.store(record_id, data, ontology=ontology)
            else:
                self.store(record_id, buf.as_bytes(), ontology=ontology)
            stages.append(
                PipelineStats(
                    "store",
                    (time.perf_counter() - t2) * 1000,
                    bytes_in=buf.size,
                )
            )
            stored = True

        # 3. Transfer — предпочитаем зашифрованный .bxos blob
        t3 = time.perf_counter()
        wire: bytes
        if stored and record_id in self._storage.registry:
            meta = self._storage.registry[record_id]
            path = meta.get("path") or ""
            if path and os.path.exists(path):
                with open(path, "rb") as f:
                    wire = f.read()
            else:
                wire = meta.get("wire") or buf.as_bytes()
        else:
            wire = buf.as_bytes()

        tr = self.optical.transfer(
            wire, target_dc, distance_km=distance_km, **link_kwargs
        )
        stages.append(
            PipelineStats(
                "transfer",
                (time.perf_counter() - t3) * 1000,
                bytes_in=len(wire),
                bytes_out=tr.bytes_sent,
            )
        )

        return InterDCResult(
            record_id=record_id,
            copied=True,
            stored=stored,
            transferred=tr.success,
            target_dc=target_dc,
            total_latency_ms=(time.perf_counter() - t0) * 1000,
            stages=stages,
            optical_decision=tr.decision,
            checksum=buf.checksum(),
        )

    def triage_and_replicate(
        self,
        batch_scores: np.ndarray,
        payloads: List[bytes],
        record_ids: List[str],
        target_dc: str,
        distance_km: float = 120.0,
    ) -> List[InterDCResult]:
        """
        L3 triage:
          -1 REJECT  → skip
           0 CLARIFY → still send
           1 OK      → copy + store + transfer
        """
        decisions = _l3_triage(batch_scores.astype(np.float64))
        results: List[InterDCResult] = []
        for dec, payload, rid in zip(decisions, payloads, record_ids):
            if dec == -1:
                results.append(
                    InterDCResult(
                        record_id=rid,
                        copied=False,
                        stored=False,
                        transferred=False,
                        target_dc=target_dc,
                        total_latency_ms=0.0,
                        stages=[],
                        optical_decision="SKIPPED",
                        checksum="",
                    )
                )
                continue
            results.append(
                self.replicate(
                    rid, payload, target_dc, distance_km=distance_km
                )
            )
        return results
# ==============================================================================
# 6. FastAPI (optional)
# ==============================================================================

if HAS_FASTAPI:
    app = FastAPI(
        title="Universal Datacenter Node V24.1 Inter-DC",
        description="Fast Copy → BXOS+AES-GCM → Optical Transfer",
        version="24.1-InterDC",
    )
    _node = UniversalDatacenterNodeV24(dc_id="DC-A")

    class ReplicateRequest(BaseModel):
        record_id: str
        payload_b64: str
        target_dc: str = "DC-B"
        distance_km: float = 120.0
        store_locally: bool = True

    @app.get("/")
    def root():
        return {
            "author": "Vladimir Zavodiuk",
            "rights": "All rights reserved",
            "project": "Universal Datacenter Node V24.1 Inter-DC",
            "version": "24.1",
            "ternary": "[-1, 0, 1]",
            "capabilities": [
                "Zero-copy FastBuffer",
                "BXOS + AES-256-GCM (FS-resilient)",
                "Ternary BXOS guard + L3 triage",
                "Optical Inter-DC (CD/Kerr/OSNR)",
                "Selective replication",
            ],
        }

    @app.post("/replicate")
    def api_replicate(req: ReplicateRequest):
        try:
            data = base64.b64decode(req.payload_b64)
            r = _node.replicate(
                req.record_id,
                data,
                req.target_dc,
                distance_km=req.distance_km,
                store_locally=req.store_locally,
            )
            return {
                "record_id": r.record_id,
                "copied": r.copied,
                "stored": r.stored,
                "transferred": r.transferred,
                "target_dc": r.target_dc,
                "total_latency_ms": round(r.total_latency_ms, 4),
                "optical_decision": r.optical_decision,
                "checksum": r.checksum,
                "stages": [
                    {
                        "stage": s.stage,
                        "ms": round(s.latency_ms, 4),
                        "in": s.bytes_in,
                        "out": s.bytes_out,
                    }
                    for s in r.stages
                ],
            }
        except Exception as e:
            raise HTTPException(status_code=400, detail=str(e))

    @app.get("/records")
    def list_records():
        return {
            "ids": _node._storage.list_ids(),
            "count": len(_node._storage.registry),
        }


# ==============================================================================
# 7. DEMO + BENCHMARK
# ==============================================================================

def run_demo_and_bench() -> Dict[str, Any]:
    print("=" * 72)
    print("Universal Datacenter Node V24.1 Inter-DC")
    print("Copy → Store (BXOS+AES-GCM, ternary) → Optical Transfer")
    print("Author: Vladimir Zavodiuk | All Rights Reserved")
    print("=" * 72)

    node = UniversalDatacenterNodeV24(
        dc_id="DC-SOFIA", storage_dir="./dc_v24_1_storage"
    )

    # --- ternary guard ---
    noisy = np.array([[-2, -1, 0, 1, 2], [3, -3, 0.5, -0.5, 1.7]], dtype=np.float64)
    clean = _bxos_guard(noisy)
    assert set(np.unique(clean)).issubset({-1, 0, 1})

    # --- fast copy ---
    src = np.random.randint(-1, 2, (512, 512), dtype=np.int8)
    t0 = time.perf_counter()
    buf = node.copy_in(src)
    copy_ms = (time.perf_counter() - t0) * 1000

    # --- store + load roundtrip (ndarray → ternary path) ---
    rid = "amber-matrix-001"
    t0 = time.perf_counter()
    path = node.store(rid, src, ontology=[1, 0, -1, 0, 1])
    store_ms = (time.perf_counter() - t0) * 1000
    loaded = node.load(rid)
    assert np.array_equal(loaded, src), "roundtrip failed"

    # --- transfer ---
    t0 = time.perf_counter()
    tr = node.transfer_to(buf, target_dc="DC-VARNA", distance_km=380.0)
    xfer_ms = (time.perf_counter() - t0) * 1000

    # --- full replicate ---
    payload = b"ZVD-TETRA345-" + os.urandom(32 * 1024)
    t0 = time.perf_counter()
    res = node.replicate(
        "repl-24.1-001",
        payload,
        target_dc="DC-PLOVDIV",
        distance_km=150.0,
    )
    repl_ms = (time.perf_counter() - t0) * 1000

    # --- batch triage ---
    scores = np.random.rand(24, 4)
    scores[:8] *= 0.2  # force REJECT
    payloads = [os.urandom(2048) for _ in range(24)]
    rids = [f"batch-{i:03d}" for i in range(24)]
    t0 = time.perf_counter()
    batch_res = node.triage_and_replicate(
        scores, payloads, rids, "DC-BURGAS", distance_km=200.0
    )
    batch_ms = (time.perf_counter() - t0) * 1000
    transferred = sum(1 for r in batch_res if r.transferred)
    skipped = sum(1 for r in batch_res if r.optical_decision == "SKIPPED")

    report = {
        "status": "ok",
        "version": "24.1-INTERDC",
        "dc_id": node.dc_id,
        "aes_gcm": HAS_AESGCM,
        "numba": HAS_NUMBA,
        "ternary_guard_ok": True,
        "copy_512x512_ms": round(copy_ms, 4),
        "store_ms": round(store_ms, 4),
        "transfer_ms": round(xfer_ms, 4),
        "replicate_32KB_ms": round(repl_ms, 4),
        "batch_24_ms": round(batch_ms, 4),
        "batch_transferred": transferred,
        "batch_skipped_reject": skipped,
        "optical_decision": res.optical_decision,
        "stored_records": len(node._storage.list_ids()),
        "stages": [
            {"stage": s.stage, "ms": round(s.latency_ms, 4)} for s in res.stages
        ],
    }
    print(json.dumps(report, indent=2, ensure_ascii=False))
    print("=" * 72)
    return report


if __name__ == "__main__":
    run_demo_and_bench()
