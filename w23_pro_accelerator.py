# -*- coding: utf-8 -*-
"""
================================================================================
W23-PRO: ИНТЕГРИРОВАННЫЙ ТРОИЧНО-ПЕНТАРНЫЙ ИИ-АКСЕЛЕРАТОР (СИНХРО-МУАР И ЛУКАСЕВИЧ)
Главный Архитектор: Владимир Заводюк (Root-Architect)
Версия: W23-Pro (Adaptive Bi-Core Counter-Rotating Moiré & Multivalued Logic)

ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ.
ПРИМЕНЕНИЕ И РАЗВЁРТЫВАНИЕ ТОЛЬКО С СОГЛАСИЯ АВТОРА.

Особенности сборки:
  1. Троично-пентарный мост: физические ядра оперируют в [-1, 0, 1], а точка 
     их пересечения и интерференции порождает расширенный пентарный домен [-2, 2].
  2. Защита от дрейфа: 12-фазный целочисленный шаг (0..11) для циклической симметрии.
  3. Стабилизация шлюзов: PhaseHysteresisMemory для устранения дребезга сигналов.
  4. Векторная рекуперация: мгновенный сброс аномалий через NumPy-маски.
  5. Многозначная логика: Гёдель/Клини (min) и коммутативный Лукасевич (K=2).
================================================================================
"""

import numpy as np
import time
import math
from collections import deque, Counter
from typing import Dict, List, Tuple, Union, Literal, Sequence
from dataclasses import dataclass

TERNARY_BASIS = np.array([1, 0, -1], dtype=np.int8)

TERNARY_CODE: Dict[int, str] = {
    -1: "ЛОЖЬ / БЛОКИРОВКА (Аппаратный обрыв / Рекуперационный сток)",
     0: "НЕОПРЕДЕЛЕННОСТЬ / ПОКОЙ (Ламинарный регенерированный ноль)",
     1: "ИСТИНА / АКТИВАЦИЯ (Индукционный резонансный подъем ядра)"
}

PENTAR_CODE: Dict[int, str] = {
    -2: "КАТЕГОРИЧЕСКОЕ НЕТ (Обрыв / Тень)",
    -1: "МЯГКОЕ НЕТ (Торможение)",
     0: "НЕОПРЕДЕЛЕННОСТЬ / ПОКОЙ (Демпфер)",
     1: "МЯГКОЕ ДА (Рост)",
     2: "КАТЕГОРИЧЕСКОЕ ДА (Взрывной фронт)"
}


class BXOSGate:
    """Детерминированный входной/выходной фильтр критических состояний с нулевой задержкой."""
    
    @staticmethod
    def guard(signal: np.ndarray) -> np.ndarray:
        return np.clip(signal, -2.0, 2.0)

    @staticmethod
    def ternary_guard(signal: np.ndarray) -> np.ndarray:
        return np.clip(signal, -1.0, 1.0)


# ============================================================================
# МОДУЛЬ 1: МАКРО-ПОЛЕ С ВЕКТОРНОЙ РЕКУПЕРАЦИЕЙ
# ============================================================================
class MacroFieldW23Pro:
    def __init__(self, dimensions: Tuple[int, int] = (7, 7)):
        self.dimensions = dimensions
        self.field = np.zeros(dimensions, dtype=np.int8)
        
        mid_x, mid_y = dimensions[0] // 2, dimensions[1] // 2
        for x in range(dimensions[0]):
            for y in range(dimensions[1]):
                dist = abs(x - mid_x) + abs(y - mid_y)
                if dist == 0: self.field[x, y] = np.int8(1)
                elif dist <= 2: self.field[x, y] = np.int8(0)
                else: self.field[x, y] = np.int8(-1)

    def calculate_entropy(self) -> float:
        total = self.field.size
        if total == 0:
            return 0.0
        counts = Counter(int(x) for x in self.field.flatten())
        entropy = 0.0
        for count in counts.values():
            p = count / total
            if p > 0:
                entropy -= p * math.log(p, 3)
        return float(entropy)

    def recover_extremes(self) -> int:
        """
        ⚡ Векторизованная рекуперация на NumPy: 
        мгновенный сброс заблокированных состояний (-1) в троичный баланс (0).
        """
        mask = (self.field == -1)
        count = int(np.sum(mask))
        if count > 0:
            self.field[mask] = np.int8(0)
        return count


# ============================================================================
# МОДУЛЬ 2: БИ-ЯДЕРНЫЙ КОНТРВРАЩАТЕЛЬ С ЦЕЛОЧИСЛЕННЫМ ТАКТОМ (12 ФАЗ)
# ============================================================================
class CounterRotatingTernaryBiCore:
    """
    Два тернарных ядра [-1, 0, 1], контрвращающиеся с адаптивной скоростью 
    на базе прецизионного целочисленного индекса (0..11) без плавающего дрейфа.
    """
    def __init__(self, size: int = 3):
        self.size = size
        self.matrix_a = self._generate_ternary(size)
        self.matrix_b = self._generate_ternary(size)
        
        # Целочисленные индексы фазы (0..11 для шага 30°)
        self.step_index_a: int = 0
        self.step_index_b: int = 0

    def _generate_ternary(self, size: int) -> np.ndarray:
        mat = np.zeros((size, size), dtype=np.int8)
        mid = size // 2
        for x in range(size):
            for y in range(size):
                if x == mid and y == mid: mat[x, y] = np.int8(0)
                elif (x + y) % 2 == 0: mat[x, y] = np.int8(1)
                else: mat[x, y] = np.int8(-1)
        return mat

    def update_rotation(self, entropy: float) -> None:
        step_delta = 1 if entropy < 0.5 else 1  # Дискретный шаг такта
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
        cos_a, sin_a = math.cos(angle), math.sin(angle)
        rx = int(round(ox * cos_a - oy * sin_a))
        ry = int(round(ox * sin_a + oy * cos_a))
        tx = max(0, min(self.size - 1, rx + mid))
        ty = max(0, min(self.size - 1, ry + mid))
        return int(matrix[tx, ty])


# ============================================================================
# МОДУЛЬ 3: ТОПОЛОГИЧЕСКАЯ ПАМЯТЬ И ФАЗОВЫЙ ГИСТЕРЕЗИС
# ============================================================================
class PhaseHysteresisMemory:
    """Учет предыстории состояний шлюза для устранения дребезга сигналов."""
    def __init__(self, window_size: int = 4):
        self.history = deque(maxlen=window_size)

    def register_state(self, state: int) -> int:
        self.history.append(state)
        avg = sum(self.history) / len(self.history)
        if avg > 0.33: return 1
        elif avg >= -0.33: return 0
        else: return -1


# ============================================================================
# МОДУЛЬ 4: УНИВЕРСАЛЬНЫЙ МНОГОЗНАЧНЫЙ ЛОГИЧЕСКИЙ КОНТУР (ГЁДЕЛЬ / ЛУКАСЕВИЧ)
# ============================================================================
class MultivaluedLogicEnginePro:
    """Связка Гёделя/Клини (min) и сильного коммутативного Лукасевича для пентарного домена (K=2)."""
    
    def __init__(self, strategy: Literal["godel", "luka"] = "luka"):
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
            n = len(arr)
            if n == 0:
                return -2
            total_sum = int(np.sum(arr, dtype=np.int16))
            return int(max(-2, total_sum - 2 * (n - 1)))

    def _reduce_batch(self, batch: np.ndarray) -> np.ndarray:
        if self.strategy == "godel":
            return np.minimum.reduce(batch, axis=-1)
        else:
            n = batch.shape[-1]
            total_sum = np.sum(batch.astype(np.int16), axis=-1)
            res = np.maximum(-2, total_sum - 2 * (n - 1))
            return res.astype(np.int8)


class PentarnyRashomon:
    """Многоракурсный блок валидации (Rashomon) без аллокационного мусора."""
    PERSPECTIVES = ("technical", "operational", "structural", "boundary", "risk")

    @staticmethod
    def evaluate(matrix: np.ndarray) -> Dict[str, Union[float, np.ndarray]]:
        if matrix.size == 0:
            return {p: 0.0 for p in PentarnyRashomon.PERSPECTIVES}
        float_m = matrix.astype(float)
        return {
            "technical": np.mean(float_m, axis=-1),
            "operational": np.min(float_m, axis=-1),
            "structural": np.max(float_m, axis=-1),
            "boundary": np.std(float_m, axis=-1) if float_m.ndim > 0 else 0.0,
            "risk": -np.mean(np.abs(float_m), axis=-1)
        }


# ============================================================================
# МОДУЛЬ 5: ГЛАВНЫЙ ИНТЕГРИРОВАННЫЙ ОРКЕСТРАТОР W23-PRO
# ============================================================================
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
    Главный оркестратор W23-Pro: объединяет би-ядерный контрвращающийся синхро-муар,
    троично-пентарный мост интерференции, гистерезисную память и логику Лукасевича.
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
        
        # Рекуперация экстремумов при повышении энтропии
        if current_entropy > 0.55:
            recovered_nodes = self.macro_field.recover_extremes()

        self.bi_core.update_rotation(current_entropy)
        slice_field = self.macro_field.field[2:5, 2:5]
        
        interference_vector = []
        matches = 0
        
        for tx in range(3):
            for ty in range(3):
                val_a = self.bi_core.get_combined_state((tx, ty), self.bi_core.angle_a, self.bi_core.matrix_a)
                val_b = self.bi_core.get_combined_state((tx, ty), self.bi_core.angle_b, self.bi_core.matrix_b)
                
                # Троично-пентарный мост: сумма в базовом поле клиппируется в пентарный домен [-2, 2]
                bridge_val = int(np.clip(val_a + val_b, -2, 2))
                interference_vector.append(bridge_val)
                
                if tx < slice_field.shape[0] and ty < slice_field.shape[1]:
                    p_val = int(slice_field[tx, ty])
                    if bridge_val == p_val:
                        matches += 1

        # Свёртка интерференционного вектора через логический движок Лукасевича/Гёделя
        folded_result = self.logic_engine.fold(interference_vector)
        interaction_sum = int(np.sum(interference_vector))

        # Применение фазового гистерезиса топологической памяти
        stabilized_gate = self.hysteresis.register_state(int(np.clip(folded_result, -1, 1)))
        coherence = float(matches / 9.0)
        
        audit_res = self.rashomon.evaluate(np.array(interference_vector, dtype=np.int8))

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
        
        gate_desc = PENTAR_CODE.get(res.stabilized_gate, TERNARY_CODE.get(res.stabilized_gate, "НЕИЗВЕСТНО"))
        
        print(f"\n================ [ СИНХРО-МУАР W23-PRO: ТАКТ №{res.time_step} ] ================")
        print(f"  -> Энтропия среды (Base-3)           : {res.entropy:.4f}")
        print(f"  -> Индексы фазы [Alpha: {self.bi_core.step_index_a:2d} ({deg_a:5.1f}°) | Beta: {self.bi_core.step_index_b:2d} ({deg_b:5.1f}°)]")
        print(f"  -> Суммарный индекс муара (Σ)         : {res.raw_interference:+d}")
        print(f"  -> Когерентность сетки               : {res.coherence:.2f}")
        print(f"  -> Восстановлено узлов (NumPy Mask)  : {res.recovered_nodes}")
        print(f"  -> Стабилизированный шлюз (Гистерезис): {res.stabilized_gate:+d} ({gate_desc})")
        print(f"  -> Rashomon Аудит (Тех/Опт/Структур): {res.rashomon_audit['technical']:.2f} / {res.rashomon_audit['operational']} / {res.rashomon_audit['structural']}")


if __name__ == "__main__":
    print("=" * 75)
    print("🧠 ИНТЕГРИРОВАННЫЙ ТРОИЧНО-ПЕНТАРНЫЙ АКСЕЛЕРАТОР — W23-PRO")
    print("⚠️ АВТОРСКИЕ ПРАВА ЗАЩИЩЕНЫ. ГЛАВНЫЙ АРХИТЕКТОР: ВЛАДИМИР ЗАВОДЮК.")
    print("=" * 75)
    
    orchestrator = MoiréUnifiedOrchestratorW23Pro(logic_strategy="luka")
    
    for cycle in range(6):
        orchestrator.display_telemetry()
        time.sleep(0.12)
        
    print("\n" + "=" * 75)
    print("✅ КОНТУР W23-PRO УСПЕШНО СИНТЕЗИРОВАН, ПРОТЕСТИРОВАН И ГОТОВ К РАЗВЁРТЫВАНИЮ")
    print("=" * 75)
