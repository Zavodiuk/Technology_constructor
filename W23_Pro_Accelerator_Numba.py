#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# All rights reserved
# Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
# Author: Vladimir Zavodiuk (Root-Architect)
# Version: W23-Pro + Numba

"""
================================================================================
W23-PRO + NUMBA
Интегрированный троично-пентарный ИИ-акселератор
(Синхро-муар + Лукасевич / Гёдель) с JIT-ускорением
================================================================================

Главный Архитектор: Владимир Заводюк

Особенности:
  1. Троично-пентарный мост: ядра в [-1, 0, 1], интерференция → [-2, 2]
  2. 12-фазный целочисленный такт (защита от дрейфа)
  3. PhaseHysteresisMemory — устранение дребезга
  4. Векторная рекуперация аномалий
  5. Многозначная логика: Гёдель/Клини + коммутативный Лукасевич (K=2)
  6. Numba JIT на критическом пути (логика, guard, энтропия, циклы)

ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ.
================================================================================
"""

from __future__ import annotations

import numpy as np
import time
import math
from collections import deque, Counter
from typing import Dict, List, Tuple, Union, Literal, Sequence
from dataclasses import dataclass
from numba import njit, prange

# ==============================================================================
# Константы
# ==============================================================================

TERNARY_BASIS = np.array([1, 0, -1], dtype=np.int8)

TERNARY_CODE: Dict[int, str] = {
    -1: "ЛОЖЬ / БЛОКИРОВКА",
     0: "НЕОПРЕДЕЛЕННОСТЬ / ПОКОЙ",
     1: "ИСТИНА / АКТИВАЦИЯ"
}

PENTAR_CODE: Dict[int, str] = {
    -2: "КАТЕГОРИЧЕСКОЕ НЕТ",
    -1: "МЯГКОЕ НЕТ",
     0: "НЕОПРЕДЕЛЕННОСТЬ / ПОКОЙ",
     1: "МЯГКОЕ ДА",
     2: "КАТЕГОРИЧЕСКОЕ ДА"
}


# ==============================================================================
# Numba-ядра
# ==============================================================================

@njit(cache=True, fastmath=True)
def bxos_guard_numba(signal: np.ndarray) -> np.ndarray:
    """Клип в [-2, 2], возврат int8."""
    out = np.empty(signal.shape, dtype=np.int8)
    flat_in = signal.ravel()
    flat_out = out.ravel()
    for i in range(flat_in.size):
        v = flat_in[i]
        if v > 2.0:
            flat_out[i] = 2
        elif v < -2.0:
            flat_out[i] = -2
        else:
            flat_out[i] = np.int8(v)
    return out


@njit(cache=True, fastmath=True)
def ternary_guard_numba(signal: np.ndarray) -> np.ndarray:
    """Клип в [-1, 1], возврат int8."""
    out = np.empty(signal.shape, dtype=np.int8)
    flat_in = signal.ravel()
    flat_out = out.ravel()
    for i in range(flat_in.size):
        v = flat_in[i]
        if v > 1.0:
            flat_out[i] = 1
        elif v < -1.0:
            flat_out[i] = -1
        else:
            flat_out[i] = np.int8(v)
    return out


@njit(cache=True, fastmath=True)
def godel_reduce_1d(arr: np.ndarray) -> int:
    if arr.size == 0:
        return 0
    m = arr[0]
    for i in range(1, arr.size):
        if arr[i] < m:
            m = arr[i]
    return int(m)


@njit(cache=True, fastmath=True)
def luka_reduce_1d(arr: np.ndarray) -> int:
    n = arr.size
    if n == 0:
        return -2
    total = 0
    for i in range(n):
        total += arr[i]
    val = total - 2 * (n - 1)
    return val if val > -2 else -2


@njit(cache=True, parallel=True, fastmath=True)
def godel_reduce_batch(batch: np.ndarray) -> np.ndarray:
    n_rows, n_cols = batch.shape
    out = np.empty(n_rows, dtype=np.int8)
    for i in prange(n_rows):
        m = batch[i, 0]
        for j in range(1, n_cols):
            if batch[i, j] < m:
                m = batch[i, j]
        out[i] = m
    return out


@njit(cache=True, parallel=True, fastmath=True)
def luka_reduce_batch(batch: np.ndarray) -> np.ndarray:
    n_rows, n_cols = batch.shape
    out = np.empty(n_rows, dtype=np.int8)
    for i in prange(n_rows):
        total = 0
        for j in range(n_cols):
            total += batch[i, j]
        val = total - 2 * (n_cols - 1)
        out[i] = val if val > -2 else -2
    return out


@njit(cache=True, fastmath=True)
def recover_extremes_numba(field: np.ndarray) -> int:
    """Сброс всех -1 в 0. Возвращает количество восстановленных узлов."""
    count = 0
    flat = field.ravel()
    for i in range(flat.size):
        if flat[i] == -1:
            flat[i] = 0
            count += 1
    return count


@njit(cache=True, fastmath=True)
def entropy_base3_numba(field: np.ndarray) -> float:
    """Энтропия по основанию 3."""
    total = field.size
    if total == 0:
        return 0.0
    # Считаем частоты для -1, 0, 1 (и на всякий случай другие)
    c_m1 = 0
    c_0 = 0
    c_1 = 0
    c_other = 0
    flat = field.ravel()
    for i in range(flat.size):
        v = flat[i]
        if v == -1:
            c_m1 += 1
        elif v == 0:
            c_0 += 1
        elif v == 1:
            c_1 += 1
        else:
            c_other += 1

    entropy = 0.0
    for cnt in (c_m1, c_0, c_1, c_other):
        if cnt > 0:
            p = cnt / total
            entropy -= p * math.log(p) / math.log(3.0)
    return entropy


# ==============================================================================
# Высокоуровневые классы
# ==============================================================================

class BXOSGate:
    @staticmethod
    def guard(signal: np.ndarray) -> np.ndarray:
        arr = np.asarray(signal)
        return bxos_guard_numba(arr.astype(np.float64, copy=False))

    @staticmethod
    def ternary_guard(signal: np.ndarray) -> np.ndarray:
        arr = np.asarray(signal)
        return ternary_guard_numba(arr.astype(np.float64, copy=False))


class MacroFieldW23Pro:
    def __init__(self, dimensions: Tuple[int, int] = (7, 7)):
        self.dimensions = dimensions
        self.field = np.zeros(dimensions, dtype=np.int8)

        mid_x, mid_y = dimensions[0] // 2, dimensions[1] // 2
        for x in range(dimensions[0]):
            for y in range(dimensions[1]):
                dist = abs(x - mid_x) + abs(y - mid_y)
                if dist == 0:
                    self.field[x, y] = np.int8(1)
                elif dist <= 2:
                    self.field[x, y] = np.int8(0)
                else:
                    self.field[x, y] = np.int8(-1)

    def calculate_entropy(self) -> float:
        return float(entropy_base3_numba(self.field))

    def recover_extremes(self) -> int:
        return int(recover_extremes_numba(self.field))


class CounterRotatingTernaryBiCore:
    """
    Два тернарных ядра, контрвращающиеся.
    Фаза хранится целочисленно (0..11), без накопления ошибки.
    """
    def __init__(self, size: int = 3):
        self.size = size
        self.matrix_a = self._generate_ternary(size)
        self.matrix_b = self._generate_ternary(size)
        self.step_index_a: int = 0
        self.step_index_b: int = 0

    def _generate_ternary(self, size: int) -> np.ndarray:
        mat = np.zeros((size, size), dtype=np.int8)
        mid = size // 2
        for x in range(size):
            for y in range(size):
                if x == mid and y == mid:
                    mat[x, y] = np.int8(0)
                elif (x + y) % 2 == 0:
                    mat[x, y] = np.int8(1)
                else:
                    mat[x, y] = np.int8(-1)
        return mat

    def update_rotation(self, entropy: float) -> None:
        step_delta = 1
        self.step_index_a = (self.step_index_a + step_delta) % 12
        self.step_index_b = (self.step_index_b - int(step_delta * 1.5)) % 12

    @property
    def angle_a(self) -> float:
        return self.step_index_a * (math.pi / 6.0)

    @property
    def angle_b(self) -> float:
        return self.step_index_b * (math.pi / 6.0)

    def get_combined_state(self, local_coord: Tuple[int, int], angle: float, matrix: np.ndarray) -> int:
        x, y = local_coord
        mid = self.size // 2
        ox, oy = x - mid, y - mid
        cos_a = math.cos(angle)
        sin_a = math.sin(angle)
        rx = int(round(ox * cos_a - oy * sin_a))
        ry = int(round(ox * sin_a + oy * cos_a))
        tx = max(0, min(self.size - 1, rx + mid))
        ty = max(0, min(self.size - 1, ry + mid))
        return int(matrix[tx, ty])


class PhaseHysteresisMemory:
    def __init__(self, window_size: int = 4):
        self.history = deque(maxlen=window_size)

    def register_state(self, state: int) -> int:
        self.history.append(state)
        avg = sum(self.history) / len(self.history)
        if avg > 0.33:
            return 1
        elif avg >= -0.33:
            return 0
        else:
            return -1


class MultivaluedLogicEnginePro:
    def __init__(self, strategy: Literal["godel", "luka"] = "luka"):
        if strategy not in ("godel", "luka"):
            raise ValueError("strategy must be 'godel' or 'luka'")
        self.strategy = strategy

    def fold(self, data: Union[Sequence[int], np.ndarray]) -> Union[int, np.ndarray]:
        arr = np.asarray(data, dtype=np.int8)
        clean = BXOSGate.guard(arr)

        if clean.ndim == 1:
            return self._reduce_1d(clean)
        return self._reduce_batch(clean)

    def _reduce_1d(self, arr: np.ndarray) -> int:
        if self.strategy == "godel":
            return godel_reduce_1d(arr)
        return luka_reduce_1d(arr)

    def _reduce_batch(self, batch: np.ndarray) -> np.ndarray:
        if self.strategy == "godel":
            return godel_reduce_batch(batch)
        return luka_reduce_batch(batch)


class PentarnyRashomon:
    PERSPECTIVES = ("technical", "operational", "structural", "boundary", "risk")

    @staticmethod
    def evaluate(matrix: np.ndarray) -> Dict[str, Union[float, np.ndarray]]:
        if matrix.size == 0:
            return {p: 0.0 for p in PentarnyRashomon.PERSPECTIVES}
        float_m = matrix.astype(float)
        return {
            "technical": float(np.mean(float_m, axis=-1)),
            "operational": float(np.min(float_m, axis=-1)),
            "structural": float(np.max(float_m, axis=-1)),
            "boundary": float(np.std(float_m, axis=-1)) if float_m.ndim > 0 else 0.0,
            "risk": float(-np.mean(np.abs(float_m), axis=-1))
        }


@dataclass
class W23ProResult:
    time_step: int
    stabilized_gate: int
    raw_interference: int
    entropy: float
    coherence: float
    recovered_nodes: int
    rashomon_audit: Dict[str, Union[float, np.ndarray]]


class MoiréUnifiedOrchestratorW23Pro:
    """
    Главный оркестратор W23-Pro + Numba.
    """
    def __init__(self, logic_strategy: Literal["godel", "luka"] = "luka"):
        self.macro_field = MacroFieldW23Pro((7, 7))
        self.bi_core = CounterRotatingTernaryBiCore(size=3)
        self.hysteresis = PhaseHysteresisMemory(window_size=4)
        self.logic_engine = MultivaluedLogicEnginePro(strategy=logic_strategy)
        self.rashomon = PentarnyRashomon()
        self.time_step: int = 0

    def execute_sync_cycle(self) -> W23ProResult:
        self.time_step += 1
        current_entropy = self.macro_field.calculate_entropy()
        recovered_nodes = 0

        if current_entropy > 0.55:
            recovered_nodes = self.macro_field.recover_extremes()

        self.bi_core.update_rotation(current_entropy)
        slice_field = self.macro_field.field[2:5, 2:5]

        interference_vector = []
        matches = 0

        for tx in range(3):
            for ty in range(3):
                val_a = self.bi_core.get_combined_state(
                    (tx, ty), self.bi_core.angle_a, self.bi_core.matrix_a
                )
                val_b = self.bi_core.get_combined_state(
                    (tx, ty), self.bi_core.angle_b, self.bi_core.matrix_b
                )
                # Интерференция (пентарный мост)
                inter = int(np.clip(val_a + val_b, -2, 2))
                interference_vector.append(inter)
                if val_a == val_b:
                    matches += 1

        folded_result = self.logic_engine.fold(interference_vector)
        interaction_sum = int(np.sum(interference_vector))

        stabilized_gate = self.hysteresis.register_state(
            int(np.clip(folded_result, -1, 1))
        )
        coherence = float(matches / 9.0)

        audit_res = self.rashomon.evaluate(
            np.array(interference_vector, dtype=np.int8)
        )

        return W23ProResult(
            time_step=self.time_step,
            stabilized_gate=stabilized_gate,
            raw_interference=interaction_sum,
            entropy=current_entropy,
            coherence=coherence,
            recovered_nodes=recovered_nodes,
            rashomon_audit=audit_res
        )

    def display_telemetry(self) -> None:
        res = self.execute_sync_cycle()
        deg_a = math.degrees(self.bi_core.angle_a) % 360
        deg_b = math.degrees(self.bi_core.angle_b) % 360

        gate_desc = PENTAR_CODE.get(
            res.stabilized_gate,
            TERNARY_CODE.get(res.stabilized_gate, "НЕИЗВЕСТНО")
        )

        print(f"\n================ [ СИНХРО-МУАР W23-PRO + NUMBA: ТАКТ №{res.time_step} ] ================")
        print(f"  -> Энтропия среды (Base-3)           : {res.entropy:.4f}")
        print(f"  -> Индексы фазы [Alpha: {self.bi_core.step_index_a:2d} ({deg_a:5.1f}°) | Beta: {self.bi_core.step_index_b:2d} ({deg_b:5.1f}°)]")
        print(f"  -> Суммарный индекс муара (Σ)         : {res.raw_interference:+d}")
        print(f"  -> Когерентность сетки               : {res.coherence:.2f}")
        print(f"  -> Восстановлено узлов               : {res.recovered_nodes}")
        print(f"  -> Стабилизированный шлюз            : {res.stabilized_gate:+d} ({gate_desc})")
        print(f"  -> Rashomon (Тех/Опт/Структур)       : {res.rashomon_audit['technical']:.2f} / {res.rashomon_audit['operational']} / {res.rashomon_audit['structural']}")


# ==============================================================================
# Бенчмарк
# ==============================================================================

def run_benchmark(n_cycles: int = 10000) -> None:
    print("=" * 75)
    print("W23-PRO + NUMBA  |  Performance Benchmark")
    print("Author: Vladimir Zavodiuk")
    print("=" * 75)

    orch = MoiréUnifiedOrchestratorW23Pro(logic_strategy="luka")

    # Warmup JIT
    print("[*] Warming up Numba JIT ...")
    for _ in range(100):
        orch.execute_sync_cycle()
    print("    JIT ready.")

    # Full cycle
    t0 = time.perf_counter()
    for _ in range(n_cycles):
        orch.execute_sync_cycle()
    dt = time.perf_counter() - t0

    print(f"\nFull execute_sync_cycle : {n_cycles/dt:,.0f} cycles/sec   ({dt/n_cycles*1e6:.1f} µs/cycle)")

    # Logic only
    data = np.random.randint(-2, 3, size=(200_000, 5), dtype=np.int8)
    eng_g = MultivaluedLogicEnginePro("godel")
    eng_l = MultivaluedLogicEnginePro("luka")

    _ = eng_g.fold(data[:1000])
    _ = eng_l.fold(data[:1000])

    t0 = time.perf_counter()
    _ = eng_g.fold(data)
    dt_g = time.perf_counter() - t0

    t0 = time.perf_counter()
    _ = eng_l.fold(data)
    dt_l = time.perf_counter() - t0

    print(f"Logic Godel  (200k×5)   : {dt_g*1000:.2f} ms   ({200000/dt_g/1e6:.2f} M folds/sec)")
    print(f"Logic Luka   (200k×5)   : {dt_l*1000:.2f} ms   ({200000/dt_l/1e6:.2f} M folds/sec)")

    print("\nStatus: Ready for use as high-speed LLM pre-filter layer.")
    print("=" * 75)


if __name__ == "__main__":
    import sys
    if "--benchmark" in sys.argv or len(sys.argv) == 1:
        run_benchmark()
    else:
        # Демонстрационный режим
        print("=" * 75)
        print("W23-PRO + NUMBA — Интегрированный троично-пентарный акселератор")
        print("Автор: Владимир Заводюк | All Rights Reserved")
        print("=" * 75)

        orchestrator = MoiréUnifiedOrchestratorW23Pro(logic_strategy="luka")
        for _ in range(6):
            orchestrator.display_telemetry()
            time.sleep(0.08)

        print("\n" + "=" * 75)
        print("Контур W23-PRO + Numba готов к развёртыванию")
        print("=" * 75)
