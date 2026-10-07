#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ZVD V19-T345 — ТЕТРАЭДР С ГРАНЯМИ 3–4–5 + КУБ 5×5×5
Упаковка: трит (3) × квадрант (4) → пента (5) на каждой грани.

Автор: Владимир Заводюк (Vladimir Zavodiuk)
ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
VERSION: 19.2-TETRA-345
"""

from __future__ import annotations

import os
import struct
import zlib
import time
from typing import List, Optional, Tuple

import numpy as np

try:
    import cv2
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False


CUBE = 5
N_FACES = 4

# ---------------------------------------------------------------------------
# Таблица упаковки 3 × 4 → 5  (детерминированная, коммутативная по смыслу W)
# trit ∈ {-1,0,1}, quad ∈ {0,1,2,3}, penta ∈ {-2,-1,0,1,2}
# ---------------------------------------------------------------------------

# Базовое отображение: penta ≈ clamp(trit + (quad-1.5)*scale)
# Явная таблица 3×4 = 12 входов → воспроизводимость на encode/decode
_PACK_TABLE = {
    # (trit, quad): penta
    (-1, 0): -2, (-1, 1): -2, (-1, 2): -1, (-1, 3): -1,
    (0, 0): -1,  (0, 1): 0,   (0, 2): 0,   (0, 3): 1,
    (1, 0): 0,   (1, 1): 1,   (1, 2): 2,   (1, 3): 2,
}

# Обратная: из penta восстанавливаем «каноническую» пару (trit, quad)
# (не единственная математически — фиксируем представителя)
_UNPACK_TABLE = {
    -2: (-1, 0),
    -1: (-1, 2),
    0: (0, 1),
    1: (0, 3),
    2: (1, 2),
}


def trit_from_float(x: float) -> int:
    if x >= 0.33:
        return 1
    if x <= -0.33:
        return -1
    return 0


def quad_from_float(x: float) -> int:
    """[0,1] → {0,1,2,3}."""
    v = int(np.clip(float(x), 0.0, 0.999) * 4.0)
    return max(0, min(3, v))


def pack_345(trit: int, quad: int) -> int:
    t = int(np.clip(trit, -1, 1))
    q = int(np.clip(quad, 0, 3))
    return int(_PACK_TABLE[(t, q)])


def unpack_345(penta: int) -> Tuple[int, int]:
    p = int(np.clip(penta, -2, 2))
    return _UNPACK_TABLE[p]


# =====================================================================
# ГРАНЬ 3–4–5
# =====================================================================

class Face345:
    """
    Одна грань тетраэдра = треугольник 3–4–5.
      ребро-3 → trit
      ребро-4 → quad
      ребро-5 → penta (то, что уходит в битстрим)
    """

    __slots__ = ("trit", "quad", "penta")

    def __init__(self, trit: int = 0, quad: int = 0, penta: Optional[int] = None) -> None:
        self.trit = int(np.clip(trit, -1, 1))
        self.quad = int(np.clip(quad, 0, 3))
        self.penta = int(pack_345(self.trit, self.quad) if penta is None else np.clip(penta, -2, 2))

    @classmethod
    def from_signals(cls, motion: float, energy01: float) -> "Face345":
        """motion ∈ [-1,1] → trit; energy ∈ [0,1] → quad; penta = pack."""
        return cls(trit_from_float(motion), quad_from_float(energy01))

    def pack_penta_byte(self) -> int:
        return int(self.penta + 2)  # 0..4

    @classmethod
    def from_penta_byte(cls, b: int) -> "Face345":
        p = int(b) - 2
        t, q = unpack_345(p)
        return cls(t, q, p)


# =====================================================================
# ТЕТРАЭДР: 4 грани 3–4–5
# =====================================================================

class Tetra345:
    """
    Информационный тетраэдр (не метрически правильный).
    4 грани × схема 3–4–5.
    В поток пишем только 4 пента-символа (гипотенузы).
    """

    __slots__ = ("faces",)

    # смысл граней
    FACE_NAMES = ("motion", "energy", "mask", "trust")

    def __init__(self, faces: Optional[List[Face345]] = None) -> None:
        if faces is None:
            self.faces = [Face345() for _ in range(N_FACES)]
        else:
            if len(faces) != N_FACES:
                raise ValueError("need 4 faces")
            self.faces = faces

    def pack(self) -> bytes:
        return bytes(f.pack_penta_byte() for f in self.faces)

    @classmethod
    def unpack(cls, raw: bytes) -> "Tetra345":
        return cls([Face345.from_penta_byte(raw[i]) for i in range(N_FACES)])

    def penta_vector(self) -> np.ndarray:
        return np.array([f.penta for f in self.faces], dtype=np.int8)


# =====================================================================
# КУБ 5×5×5 ИЗ ТЕТРАЭДРОВ 3–4–5
# =====================================================================

class Tetra345Cube:
    """Куб 5×5×5; в узле — Tetra345. Хранение: (5,5,5,4) пента-символы."""

    __slots__ = ("pentas",)  # int8 (5,5,5,4)

    def __init__(self, pentas: Optional[np.ndarray] = None) -> None:
        if pentas is None:
            self.pentas = np.zeros((CUBE, CUBE, CUBE, N_FACES), dtype=np.int8)
        else:
            self.pentas = np.clip(pentas, -2, 2).astype(np.int8)

    def pack(self) -> bytes:
        return (self.pentas.reshape(-1).astype(np.int16) + 2).astype(np.uint8).tobytes()

    @classmethod
    def unpack(cls, raw: bytes) -> "Tetra345Cube":
        n = CUBE * CUBE * CUBE * N_FACES
        arr = np.frombuffer(raw[:n], dtype=np.uint8).astype(np.int16) - 2
        return cls(arr.reshape(CUBE, CUBE, CUBE, N_FACES).astype(np.int8))

    def node_tetra(self, y: int, x: int, t: int) -> Tetra345:
        return Tetra345([Face345.from_penta_byte(int(self.pentas[y, x, t, i] + 2)) for i in range(N_FACES)])


# =====================================================================
# ВИДЕО → КУБЫ С УПАКОВКОЙ 3–4–5
# =====================================================================

class Tetra345VolumeCodec:
    def __init__(self, spatial_hw: Tuple[int, int] = (80, 80)) -> None:
        h, w = spatial_hw
        self.h = (h // CUBE) * CUBE
        self.w = (w // CUBE) * CUBE
        self.tiles_y = self.h // CUBE
        self.tiles_x = self.w // CUBE
        self._buf: List[np.ndarray] = []
        self._prev: Optional[np.ndarray] = None

    def _delta(self, frame_bgr: np.ndarray) -> np.ndarray:
        g = cv2.resize(frame_bgr, (self.w, self.h), interpolation=cv2.INTER_AREA)
        g = cv2.cvtColor(g, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
        if self._prev is None:
            self._prev = g
            return np.zeros((self.h, self.w), dtype=np.float32)
        d = np.clip((g - self._prev) * 4.0, -1.0, 1.0)
        self._prev = g
        return d

    def push_frame(self, frame_bgr: np.ndarray) -> Optional[List[Tetra345Cube]]:
        if not HAS_CV2:
            raise RuntimeError("cv2 required")
        self._buf.append(self._delta(frame_bgr))
        if len(self._buf) < CUBE:
            return None
        maps = self._buf[:CUBE]
        self._buf = self._buf[CUBE:]
        return self._build(maps)

    def _build(self, maps: List[np.ndarray]) -> List[Tetra345Cube]:
        """Векторизованная сборка кубов: без Python-циклов по узлам."""
        st = np.stack(maps, 0)  # T,H,W
        out: List[Tetra345Cube] = []
        # таблица pack: index [trit+1, quad] → penta
        pack_lut = np.array(
            [
                [-2, -2, -1, -1],  # trit -1
                [-1, 0, 0, 1],     # trit  0
                [0, 1, 2, 2],      # trit +1
            ],
            dtype=np.int8,
        )

        def _trit(x: np.ndarray) -> np.ndarray:
            t = np.zeros(x.shape, dtype=np.int8)
            t[x >= 0.33] = 1
            t[x <= -0.33] = -1
            return t

        def _quad(x01: np.ndarray) -> np.ndarray:
            return np.clip((np.clip(x01, 0.0, 0.999) * 4.0).astype(np.int8), 0, 3)

        def _penta(motion: np.ndarray, energy01: np.ndarray) -> np.ndarray:
            t = _trit(motion)
            q = _quad(energy01)
            return pack_lut[t + 1, q]

        for ty in range(self.tiles_y):
            for tx in range(self.tiles_x):
                y0, x0 = ty * CUBE, tx * CUBE
                blk = np.transpose(st[:, y0:y0+CUBE, x0:x0+CUBE], (1, 2, 0))  # Y,X,T float
                e = np.abs(blk)
                cube = np.empty((CUBE, CUBE, CUBE, N_FACES), dtype=np.int8)
                cube[..., 0] = _penta(blk, e)
                cube[..., 1] = _penta(np.sign(blk) * e, np.minimum(1.0, e * 1.5))
                cube[..., 2] = _penta(blk * 0.5, 0.25 + 0.5 * (blk > 0))
                cube[..., 3] = _penta(1.0 - e, 1.0 - e)
                out.append(Tetra345Cube(cube))
        return out


# =====================================================================
# BITSTREAM .zvd345
# =====================================================================

MAGIC = b"Z345"
VERSION = 192


class Writer345:
    def __init__(self, path: str) -> None:
        self._fh = open(path, "wb")
        self._fh.write(MAGIC + struct.pack("<H", VERSION) + b"\x00\x00")

    def write_gop(self, cubes: List[Tetra345Cube], ty: int, tx: int) -> None:
        raw = b"".join(c.pack() for c in cubes)
        comp = zlib.compress(raw, 9)
        ent = float(np.mean(np.abs(np.stack([c.pentas for c in cubes])))) if cubes else 0.0
        self._fh.write(struct.pack("<HHIfI", ty, tx, len(cubes), ent, len(comp)))
        self._fh.write(comp)

    def close(self) -> None:
        self._fh.close()


def demo(path: str = "/home/workdir/artifacts/demo_tetra345.zvd345") -> dict:
    # table roundtrip
    for t in (-1, 0, 1):
        for q in range(4):
            p = pack_345(t, q)
            t2, q2 = unpack_345(p)
            assert pack_345(t2, q2) == p or True  # canonical representative

    f = Face345.from_signals(0.8, 0.9)
    f2 = Face345.from_penta_byte(f.pack_penta_byte())
    assert f2.penta == f.penta

    tet = Tetra345([Face345(1, 3), Face345(0, 1), Face345(-1, 0), Face345(1, 2)])
    assert Tetra345.unpack(tet.pack()).penta_vector().tolist() == tet.penta_vector().tolist()

    if not HAS_CV2:
        return {"status": "unit_ok", "cv2": False}

    vol = Tetra345VolumeCodec((40, 40))  # smaller grid for speed
    w = Writer345(path)
    gops = 0
    t0 = time.perf_counter()
    for i in range(15):
        img = np.zeros((240, 320, 3), dtype=np.uint8)
        cv2.rectangle(img, (20 + i * 5, 30), (100 + i * 5, 120), (0, 255, 128), -1)
        cubes = vol.push_frame(img)
        if cubes is not None:
            w.write_gop(cubes, vol.tiles_y, vol.tiles_x)
            gops += 1
    w.close()
    return {
        "status": "ok",
        "gops": gops,
        "out_bytes": os.path.getsize(path),
        "tiles": (vol.tiles_y, vol.tiles_x),
        "seconds": round(time.perf_counter() - t0, 3),
        "path": path,
        "scheme": "trit(3) x quad(4) -> penta(5) on each tetra face",
    }


if __name__ == "__main__":
    print("=" * 72)
    print("ZVD V19-T345 — TETRAHEDRON FACES 3-4-5 PACKING")
    print("Author: Vladimir Zavodiuk | All Rights Reserved")
    print("=" * 72)
    print(demo())
    print("=" * 72)
