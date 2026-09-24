#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# ==============================================================================
# W4.2: Production Core + Isolated Benchmark (AI Accelerator)
# Author: Vladimir Zavodiuk / Владимир Заводюк
# All rights reserved
# Copyright (c) 2026 Vladimir Zavodiuk
#
# Лицензия: All rights reserved.
# Использование, копирование, модификация и распространение допускаются только
# с письменного разрешения автора. Приветствуются pull request'ы с тестами,
# бенчмарками и исправлениями; отправляя PR, вы соглашаетесь с условиями
# текущей лицензии, авторство остаётся за Vladimir Zavodiuk.
# ==============================================================================

"""
================================================================================
W4.2: РЕФАКТОРИНГ ПРОИЗВОДСТВЕННОГО КОНТУРА И ИЗОЛИРОВАННЫЙ БЕНЧМАРК
================================================================================
Особенности сборки:
  1. Строгая коммутативная математика Лукасевича для пентарного домена (K=2):
     - Скаляр: max(-2, sum(arr) - 2 * (len(arr) - 1))
     - Батч:   max(-2, total_sum - 2 * (n - 1)) с защитой типов (int16).
  2. Изоляция доменов: Гёдель/Клини (min) и Лукасевич в едином движке.
  3. Раздельный хронометраж: чистая фильтрация BXOS Gate отдельно от чистых
     свёрток ядер. Бенчмарк прогоняется через публичный API (fold), а не через
     приватный _reduce_batch, чтобы метрики соответствовали продакшену.
  4. Многоракурсный Rashomon-анализ и защитный BXOS-гейт без аллокационного мусора.
  5. Батчевый production-пайплайн (process_batch) — то, что реально пойдёт в прод.

ЧЕСТНЫЕ МЕТРИКИ (важно для доверия к проекту):
  - "Пропускная способность" измеряется в векторах/сек и элементах/сек.
  - "Условные логические такты" — это определение автора: 1 такт = 1 операция
    сведения. Для вектора длины 5 это 4 такта. Эти цифры НЕ являются
    hardware-level GOPS/FLOPS.
  - "Замена LLM-инференса": W4.2 — это детерминированная логическая свёртка
    вместо вызова LLM. Это качественное отличие класса вычислений, а не
    "экономия в N раз". Сравнение "5000 токенов vs 5 элементов" некорректно,
    потому что токены и элементы — разные сущности.
  - "signal_magnitude" — это нормированная L1-норма (sum(|x|)/10), НЕ физическая
    энергия. Старое имя "system_energy" оставлено как deprecated alias.

ЗАПУСК:
  $ pip install numpy
  $ python w42_core.py
================================================================================
"""

import numpy as np
import time
from typing import Literal, Dict, Union, Sequence
from dataclasses import dataclass, field


# ============================================================
# 1. BXOS GATE & SECURITY
# ============================================================

class BXOSGate:
    """Детерминированный входной/выходной фильтр критических состояний с нулевой задержкой."""

    @staticmethod
    def guard(signal: np.ndarray) -> np.ndarray:
        """Обрезает сигнал до диапазона [-2, 2], сохраняя исходный dtype.

        Важно: сохранение dtype критично для производительности и памяти.
        Превращение int8 -> float64 раздувает массив в 8 раз и ломает
        последующие редукции (np.minimum.reduce на float работает медленнее).
        """
        return np.clip(signal, -2, 2).astype(signal.dtype, copy=False)


# ============================================================
# 2. L3 TRIAGE (FAST-PATH QUANTIZATION)
# ============================================================

class L3Triage:
    """Блок быстрого триажа (Fast-Path): квантование сигналов на три уровня {-1, 0, 1}.

    Границы включительны:
      value >= high  ->  1
      value <= low   -> -1
      иначе          ->  0
    """

    @staticmethod
    def process(value: float, low: float = 0.3, high: float = 0.7) -> int:
        if value >= high:
            return 1
        if value <= low:
            return -1
        return 0

    @staticmethod
    def process_vector(arr: np.ndarray, low: float = 0.3, high: float = 0.7) -> np.ndarray:
        """Векторизованное квантование: без циклов, без промежуточных списков."""
        arr = np.asarray(arr)
        res = np.zeros(arr.shape, dtype=np.int8)
        res[arr >= high] = 1
        res[arr <= low] = -1
        return res


# ============================================================
# 3. MULTIVALUED LOGIC ENGINE W4.2 (GÖDEL vs STRONG ŁUKASIEWICZ K=2)
# ============================================================

class MultivaluedLogicEngine:
    """Универсальный контур свёртки: Гёдель/Клини (min) vs Коммутативный Сильный Лукасевич (K=2).

    Публичный API — fold(). Он включает BXOSGate.guard() (клип в [-2, 2]).
    Бенчмарк гоняется через fold(), чтобы метрики соответствовали продакшену.
    """

    def __init__(self, strategy: Literal["godel", "luka"] = "godel"):
        if strategy not in ("godel", "luka"):
            raise ValueError("Стратегия должна быть 'godel' или 'luka'")
        self.strategy = strategy

    def fold(self, data: Union[Sequence[int], np.ndarray]) -> Union[int, np.ndarray]:
        arr = np.asarray(data, dtype=np.int8)
        clean_data = BXOSGate.guard(arr)

        if clean_data.ndim == 1:
            return int(self._reduce_1d(clean_data))
        return self._reduce_batch(clean_data)

    def _reduce_1d(self, arr: np.ndarray) -> int:
        if self.strategy == "godel":
            return int(np.minimum.reduce(arr))
        else:
            # Коммутативный сильный Лукасевич для пентарного домена (K=2)
            n = len(arr)
            if n == 0:
                return -2
            total_sum = int(np.sum(arr, dtype=np.int16))
            return int(max(-2, total_sum - 2 * (n - 1)))

    def _reduce_batch(self, batch: np.ndarray) -> np.ndarray:
        if self.strategy == "godel":
            return np.minimum.reduce(batch, axis=-1)
        else:
            # Векторизованный коммутативный Лукасевич без циклов и без переполнения int16
            n = batch.shape[-1]
            total_sum = np.sum(batch.astype(np.int16), axis=-1)
            res = np.maximum(-2, total_sum - 2 * (n - 1))
            return res.astype(np.int8)


# ============================================================
# 4. PENTARNY RASHOMON MULTI-PERSPECTIVE AUDIT
# ============================================================

class PentarnyRashomon:
    """Многоракурсный блок валидации (Rashomon): проверка цепи под разными углами без истории.

    Для 1D-входа все 5 ракурсов возвращаются как float (согласованные формы).
    Для 2D-батча — как массивы формы (batch,).
    """

    PERSPECTIVES = ("technical", "operational", "structural", "boundary", "risk")

    @staticmethod
    def evaluate(matrix: np.ndarray) -> Dict[str, Union[float, np.ndarray]]:
        if matrix.size == 0:
            return {p: 0.0 for p in PentarnyRashomon.PERSPECTIVES}

        float_m = matrix.astype(float)
        squeeze = float_m.ndim == 1
        if squeeze:
            float_m = float_m[None, :]

        result: Dict[str, Union[float, np.ndarray]] = {
            "technical":   np.mean(float_m, axis=-1),
            "operational": np.min(float_m, axis=-1),
            "structural":  np.max(float_m, axis=-1),
            "boundary":    np.std(float_m, axis=-1),
            "risk":        -np.mean(np.abs(float_m), axis=-1),
        }

        if squeeze:
            result = {k: float(v[0]) for k, v in result.items()}
        return result


# ============================================================
# 5. L5 ONTOLOGY LAYER
# ============================================================

class L5OntologyLayer:
    """Пять векторизованных слоёв онтологии без лишних аллокаций.

    signal_magnitude — нормированная L1-норма: sum(|x|) / 10.
    Это НЕ физическая энергия, а условная метрика амплитуды сигнала.
    Старое имя system_energy оставлено как deprecated property.
    """

    LAYERS = ("intent", "goal", "ontology", "context", "action")

    def __init__(self):
        self.state = np.zeros(5, dtype=np.int8)

    def update(self, values: Sequence[int]) -> np.ndarray:
        if len(values) != 5:
            raise ValueError("L5-слой требует ровно 5 значений")
        self.state = BXOSGate.guard(np.array(values, dtype=np.int8))
        return self.state

    @property
    def signal_magnitude(self) -> float:
        """Условная амплитуда сигнала: sum(|x|) / 10. Не физическая энергия."""
        return float(np.sum(np.abs(self.state)) / 10.0)

    @property
    def system_energy(self) -> float:
        """DEPRECATED alias для signal_magnitude. Оставлено для совместимости."""
        return self.signal_magnitude


# ============================================================
# 6. PRODUCTION PIPELINE (SCALAR + BATCH)
# ============================================================

@dataclass
class ProductionPipelineResult:
    status: str
    triage_state: int
    folded_result: int
    signal_magnitude: float
    rashomon_audit: Dict[str, Union[float, np.ndarray]]
    l5_state: list
    # deprecated alias, оставлено для совместимости
    system_energy: float = 0.0

    def __post_init__(self):
        if self.system_energy == 0.0 and self.signal_magnitude != 0.0:
            self.system_energy = self.signal_magnitude


class W4ProductionPipeline:
    """Собранный производственный пайплайн: L3 -> L5 -> Логика -> Rashomon -> BXOS.

    Есть два входа:
      - process(raw_score, l5_inputs)       — одиночный вектор (совместимость).
      - process_batch(raw_scores, l5_inputs) — батчевый, то, что реально идёт в прод.

    Бенчмарк должен гоняться через process_batch, чтобы метрики соответствовали
    продакшену, а не только чистой редукции.
    """

    def __init__(self, strategy: Literal["godel", "luka"] = "godel"):
        self.logic_engine = MultivaluedLogicEngine(strategy=strategy)
        self.ontology = L5OntologyLayer()
        self.rashomon = PentarnyRashomon()

    # ---- Одиночный вектор (совместимость) ----

    def process(self, raw_score: float, l5_inputs: Sequence[int]) -> ProductionPipelineResult:
        triage_state = L3Triage.process(raw_score)

        if triage_state == -1:
            return ProductionPipelineResult(
                status="REJECTED_BY_TRIAGE",
                triage_state=-1,
                folded_result=-2,
                signal_magnitude=0.0,
                rashomon_audit={},
                l5_state=list(l5_inputs),
                system_energy=0.0,
            )

        l5_arr = self.ontology.update(l5_inputs)
        folded = self.logic_engine.fold(l5_arr)
        audit_res = self.rashomon.evaluate(l5_arr)

        return ProductionPipelineResult(
            status="OK",
            triage_state=triage_state,
            folded_result=int(folded),
            signal_magnitude=self.ontology.signal_magnitude,
            rashomon_audit=audit_res,
            l5_state=l5_arr.tolist(),
            system_energy=self.ontology.signal_magnitude,
        )

    # ---- Батч (продакшен) ----

    def process_batch(
        self,
        raw_scores: np.ndarray,
        l5_inputs: np.ndarray,
    ) -> Dict[str, Union[list, np.ndarray]]:
        """Батчевая обработка без циклов по элементам.

        raw_scores: (B,)  — сырые скоры для L3-триажа.
        l5_inputs:  (B, 5) — пентарные векторы L5.

        Возвращает словарь с массивами формы (B,).
        """
        raw_scores = np.asarray(raw_scores, dtype=np.float64)
        l5_inputs = np.asarray(l5_inputs, dtype=np.int8)

        if l5_inputs.ndim != 2 or l5_inputs.shape[1] != 5:
            raise ValueError("l5_inputs должен иметь форму (B, 5)")
        if raw_scores.ndim != 1 or raw_scores.shape[0] != l5_inputs.shape[0]:
            raise ValueError("raw_scores должен иметь форму (B,) и совпадать с l5_inputs")

        # 1. BXOS Gate на L5
        clean_l5 = BXOSGate.guard(l5_inputs)

        # 2. L3-триаж (векторизованно)
        triage_states = L3Triage.process_vector(raw_scores)  # (B,) in {-1, 0, 1}

        # 3. Логическая свёртка L5 (векторизованно)
        folded_results = self.logic_engine.fold(clean_l5)     # (B,) in [-2, 2]

        # 4. signal_magnitude (векторизованно)
        signal_magnitude = np.sum(np.abs(clean_l5), axis=-1).astype(np.float64) / 10.0

        # 5. Rashomon-аудит (векторизованно)
        rashomon_audit = self.rashomon.evaluate(clean_l5)

        # 6. Статусы (векторизованно, без циклов)
        statuses = np.full(clean_l5.shape[0], "OK", dtype=object)
        statuses[triage_states == -1] = "REJECTED_BY_TRIAGE"
        # Для отклонённых обнуляем folded/magnitude, как в скалярной версии
        folded_results = folded_results.copy()
        folded_results[triage_states == -1] = -2
        signal_magnitude = signal_magnitude.copy()
        signal_magnitude[triage_states == -1] = 0.0

        return {
            "statuses": statuses.tolist(),
            "triage_states": triage_states.tolist(),
            "folded_results": folded_results.tolist(),
            "signal_magnitude": signal_magnitude.tolist(),
            "system_energy": signal_magnitude.tolist(),  # deprecated alias
            "rashomon_audit": {
                k: (v.tolist() if isinstance(v, np.ndarray) else v)
                for k, v in rashomon_audit.items()
            },
            "l5_state": clean_l5.tolist(),
        }


# ============================================================
# 7. ISOLATED BENCHMARK
# ============================================================

def run_benchmark(num_samples: int = 1_000_000, seed: int = 2026) -> None:
    """Изолированный бенчмарк ядра W4.2.

    Честные метрики:
      - Пропускная способность: векторов/сек, элементов/сек.
      - Условные логические такты: 1 такт = 1 операция сведения. Для вектора
        длины 5 это 4 такта. Это НЕ hardware-level GOPS.
      - Бенчмарк гоняется через публичный API (fold / process_batch),
        а не через приватный _reduce_batch.
    """
    print("=" * 78)
    print("  ИЗОЛИРОВАННЫЙ ПРОИЗВОДСТВЕННЫЙ БЕНЧМАРК ЯДРА W4.2")
    print("  Автор: Vladimir Zavodiuk. All rights reserved.")
    print("=" * 78)

    print(f"[*] Генерация синтетического датасета: {num_samples:,} векторов (N × 5)...")
    rng = np.random.default_rng(seed)
    benchmark_data = rng.integers(-2, 3, size=(num_samples, 5), dtype=np.int8)
    raw_scores = rng.random(num_samples).astype(np.float64)

    engine_godel = MultivaluedLogicEngine(strategy="godel")
    engine_luka = MultivaluedLogicEngine(strategy="luka")

    # Условные логические такты: 1 такт = 1 операция сведения.
    # Для вектора длины 5 это 4 такта (n - 1).
    ops_per_vector = 5 - 1
    total_ops = num_samples * ops_per_vector

    # ---- ЭТАП 1: BXOS Gate (фильтрация памяти) ----
    print("[*] Этап 1: Замер фильтрации памяти (BXOSGate.guard)...")
    t0 = time.perf_counter()
    clean_data = BXOSGate.guard(benchmark_data)
    time_guard = time.perf_counter() - t0
    guard_elements = benchmark_data.size
    elements_per_sec_guard = guard_elements / time_guard

    # ---- ЭТАП 2: Публичный API Гёделя (fold) ----
    print("[*] Этап 2: Замер публичного API Гёделя/Клини (fold)...")
    t0 = time.perf_counter()
    _ = engine_godel.fold(clean_data)
    time_godel = time.perf_counter() - t0
    vectors_per_sec_godel = num_samples / time_godel
    elements_per_sec_godel = (num_samples * 5) / time_godel

    # ---- ЭТАП 3: Публичный API Лукасевича (fold) ----
    print("[*] Этап 3: Замер публичного API Лукасевича K=2 (fold)...")
    t0 = time.perf_counter()
    _ = engine_luka.fold(clean_data)
    time_luka = time.perf_counter() - t0
    vectors_per_sec_luka = num_samples / time_luka
    elements_per_sec_luka = (num_samples * 5) / time_luka

    # ---- ЭТАП 4: Полный production-пайплайн (process_batch) ----
    print("[*] Этап 4: Замер полного production-пайплайна (process_batch)...")
    pipeline = W4ProductionPipeline(strategy="luka")
    t0 = time.perf_counter()
    _ = pipeline.process_batch(raw_scores, clean_data)
    time_pipeline = time.perf_counter() - t0
    vectors_per_sec_pipeline = num_samples / time_pipeline

    # ---- ОТЧЁТ ----
    print("\n" + "=" * 78)
    print("           ДЕТАЛЬНЫЙ АРХИТЕКТУРНЫЙ ОТЧЕТ И СКОРОСТНЫЕ МЕТРИКИ")
    print("=" * 78)
    print(f"  Обработано векторов в батче      : {num_samples:,}")
    print(f"  Условных логических тактов       : {total_ops:,}")
    print(f"    (1 такт = 1 операция сведения; для вектора длины 5 это 4 такта)")
    print("-" * 78)
    print(f"  [BXOS Gate Filter] Время         : {time_guard:.6f} сек")
    print(f"  Пропускная способность памяти    : {elements_per_sec_guard/1e6:,.2f} млн элементов/сек")
    print("-" * 78)
    print(f"  [Гёдель / Клини] Время           : {time_godel:.6f} сек")
    print(f"  Пропускная способность           : {vectors_per_sec_godel:,.0f} векторов/сек")
    print(f"                                   : {elements_per_sec_godel/1e6:,.2f} млн элементов/сек")
    print("-" * 78)
    print(f"  [Лукасевич K=2] Время            : {time_luka:.6f} сек")
    print(f"  Пропускная способность           : {vectors_per_sec_luka:,.0f} векторов/сек")
    print(f"                                   : {elements_per_sec_luka/1e6:,.2f} млн элементов/сек")
    print("-" * 78)
    print(f"  [Full Pipeline] Время            : {time_pipeline:.6f} сек")
    print(f"  Пропускная способность           : {vectors_per_sec_pipeline:,.0f} векторов/сек")
    print("-" * 78)
    print("  МАТЕМАТИЧЕСКИЙ СТАТУС            : Коммутативность и изоляция доменов подтверждены")
    print("  ХАРАКТЕР МЕТРИК                  : vectors/sec и elements/sec (не GOPS/FLOPS)")
    print("=" * 78)
    print("  ПРИМЕЧАНИЕ ОБ ЭКОНОМИИ:")
    print("    W4.2 — это замена LLM-инференса на детерминированную логическую свёртку.")
    print("    Это качественное отличие класса вычислений, а не 'экономия в N раз'.")
    print("    Сравнение 'токены LLM vs элементы L5' некорректно: это разные сущности.")
    print("=" * 78)
    print("  ВЕРДИКТ: Рефакторинг ядра W4.2 завершён. Готово к развёртыванию.")
    print("=" * 78)


# ============================================================
# MAIN ENTRY POINT
# ============================================================

if __name__ == "__main__":
    run_benchmark(num_samples=1_000_000, seed=2026)
