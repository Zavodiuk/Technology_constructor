#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
PENTARNY SILICON BRIDGE SENSORY LAYER — V24.0 (PYTHON HARDWARE MOIRE & SYNC)
AUTHOR / АВТОР: Владимир Заводюк (Vladimir Zavodyuk)
ROLE / РОЛЬ: Root-Architect / Рут-Архитектор

ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ.
ПРИМЕНЕНИЕ, КОПИРОВАНИЕ И РАЗВЁРТЫВАНИЕ ТОЛЬКО С ПРЯМОГО СОГЛАСИЯ АВТОРА.
================================================================================

INSTRUCTIONS / ИНСТРУКЦИЯ ПО ЗАПУСКУ:
1. Установите numpy: pip install numpy
2. Запустите скрипт: python3 silicon_bridge_sensory.py
================================================================================
"""

import math
import time
from dataclasses import dataclass
from typing import Dict, Tuple
import numpy as np

PENTAR_SILICON_CODES = {
    -2: "HARDWARE_FAULT_BLOCK (Аппаратный сброс / Аварийный сток)",
    -1: "THERMAL_DAMPENING (Мягкое демпфирование шины)",
     0: "LAMINAR_IDLE (Ламинарный покой / Синхро-ноль)",
     1: "INDUCTION_BOOST (Индукционный подъем тактовой частоты)",
     2: "RESONANT_SUPERCONDUCTION (Резонансный квантовый мост)"
}

@dataclass
class SiliconSensorPacket:
    hardware_jitter: float
    thermal_noise: float
    bus_load: float


class SiliconBridgeSensoryLayer:
    """Аппаратный сенсорный кремниевый мост, преобразующий телеметрию в поле энтропии."""
    def __init__(self, dimensions: Tuple[int, int] = (5, 5)):
        self.dims = dimensions
        self.field = np.zeros(dimensions, dtype=np.int8)

    def inject_sensor_telemetry(self, sensor: SiliconSensorPacket):
        mid_x, mid_y = self.dims[0] // 2, self.dims[1] // 2
        for x in range(self.dims[0]):
            for y in range(self.dims[1]):
                dist = abs(x - mid_x) + abs(y - mid_y)
                base_val = when_dist(dist)
                
                # Динамическая модуляция матрицы кремния от аппаратных сенсоров
                noise_factor = -1 if sensor.hardware_jitter > 0.4 else 0
                self.field[x, y] = max(-2, min(2, base_val + noise_factor))

    def calculate_entropy(self) -> float:
        total = float(self.field.size)
        if total == 0.0:
            return 0.0
        _, counts = np.unique(self.field, return_counts=True)
        entropy = 0.0
        for count in counts:
            p = count / total
            if p > 0.0:
                entropy -= p * math.log2(p)
        return float(entropy)


def when_dist(dist: int) -> int:
    if dist == 0: return 2
    if dist == 1: return 1
    if dist == 2: return 0
    if dist == 3: return -1
    return -2


class CounterRotatingBiCore:
    """Противовращающиеся би-ядра муара для интерференционного анализа."""
    def __init__(self, size: int = 3):
        self.size = size
        self.matrix_a = self._generate_ternary(size)
        self.matrix_b = self._generate_ternary(size)
        self.angle_a = 0.0
        self.angle_b = 0.0
        self.base_velocity = math.pi / 6.0

    def _generate_ternary(self, sz: int) -> np.ndarray:
        mat = np.zeros((sz, sz), dtype=np.int8)
        mid = sz // 2
        for x in range(sz):
            for y in range(sz):
                if x == mid and y == mid:
                    mat[x, y] = 0
                elif (x + y) % 2 == 0:
                    mat[x, y] = 1
                else:
                    mat[x, y] = -1
        return mat

    def update_rotation(self, entropy: float):
        dynamic_velocity = self.base_velocity / (1.0 + entropy * 2.0)
        self.angle_a += dynamic_velocity
        self.angle_b -= dynamic_velocity * 1.5

        if self.angle_a >= 2 * math.pi: self.angle_a = 0.0
        if self.angle_b <= -2 * math.pi: self.angle_b = 0.0

    def get_combined_state(self, local_coord: Tuple[int, int], angle: float, matrix: np.ndarray) -> int:
        x, y = local_coord
        mid = self.size // 2
        ox, oy = x - mid, y - mid
        cos_a, sin_a = math.cos(angle), math.sin(angle)
        rx = round(ox * cos_a - oy * sin_a)
        ry = round(ox * sin_a + oy * cos_a)
        tx = max(0, min(self.size - 1, rx + mid))
        ty = max(0, min(self.size - 1, ry + mid))
        return int(matrix[tx, ty])


class PhaseHysteresisMemory:
    """Фазовый гистерезис для сглаживания переходных состояний шлюза."""
    def __init__(self, window_size: int = 4):
        self.window_size = window_size
        self.history = [0] * window_size

    def register_state(self, state: int) -> int:
        self.history.pop(0)
        self.history.append(state)
        avg = sum(self.history) / float(len(self.history))
        if avg > 1.2: return 2
        if avg > 0.3: return 1
        if avg >= -0.3: return 0
        if avg >= -1.2: return -1
        return -2


@dataclass
class BridgeSyncResult:
    gateway_verdict: int
    moire_sum: int
    entropy: float
    coherence: float


class SiliconBridgeOrchestrator:
    """Главный управляющий оркестратор кремниевого моста."""
    def __init__(self):
        self.sensory_layer = SiliconBridgeSensoryLayer((5, 5))
        self.bi_core = CounterRotatingBiCore(3)
        self.hysteresis = PhaseHysteresisMemory(4)
        self.time_step = 0

    def execute_bridge_cycle(self, sensor_packet: SiliconSensorPacket) -> BridgeSyncResult:
        self.time_step += 1
        self.sensory_layer.inject_sensor_telemetry(sensor_packet)
        current_entropy = self.sensory_layer.calculate_entropy()

        self.bi_core.update_rotation(current_entropy)

        # Вырезка 3x3 из центра сенсорного кремниевого поля для муара
        slice_field = self.sensory_layer.field[1:4, 1:4]

        interaction_sum = 0
        matches = 0

        for tx in range(3):
            for ty in range(3):
                val_a = self.bi_core.get_combined_state((tx, ty), self.bi_core.angle_a, self.bi_core.matrix_a)
                val_b = self.bi_core.get_combined_state((tx, ty), self.bi_core.angle_b, self.bi_core.matrix_b)
                core_interference = val_a + val_b

                p_val = int(slice_field[tx, ty])
                interaction_sum += (core_interference * p_val)
                if core_interference == p_val:
                    matches += 1

        raw_verdict = when_interaction_sum(interaction_sum)
        stabilized_verdict = self.hysteresis.register_state(raw_verdict)
        coherence = matches / 9.0

        return BridgeSyncResult(stabilized_verdict, interaction_sum, current_entropy, coherence)

    def display_telemetry(self, sensor_packet: SiliconSensorPacket):
        res = self.execute_bridge_cycle(sensor_packet)
        deg_a = (math.degrees(self.bi_core.angle_a) % 360 + 360) % 360
        deg_b = (math.degrees(self.bi_core.angle_b) % 360 + 360) % 360
        print(f"\n================ [ SILICON BRIDGE SENSORY LAYER — ТАКТ #{self.time_step} ] ================")
        print(f"  -> Входные датчики [Jitter/Thermal/Load]: {sensor_packet.hardware_jitter} / {sensor_packet.thermal_noise} / {sensor_packet.bus_load}")
        print(f"  -> Энтропия кремниевой среды: {res.entropy:.4f}")
        print(f"  -> Углы би-ядер муара [Alpha / Beta]: {deg_a:.1f}° / {deg_b:.1f}°")
        print(f"  -> Интегральный индекс интерференции (Σ): {'+' if res.moire_sum >= 0 else ''}{res.moire_sum}")
        print(f"  -> Когерентность аппаратного моста: {res.coherence:.2f}")
        print(f"  -> Стабилизированный вердикт шлюза: {'+' if res.gateway_verdict >= 0 else ''}{res.gateway_verdict} ({PENTAR_SILICON_CODES[res.gateway_verdict]})")


def when_interaction_sum(s: int) -> int:
    if s > 6: return 2
    if s > 1: return 1
    if s >= -1: return 0
    if s >= -6: return -1
    return -2


if __name__ == "__main__":
    print("=========================================================================")
    print("⚙️ PENTARNY SILICON BRIDGE SENSORY LAYER — V24.0 (PYTHON NUMPY HARDWARE MOIRE)")
    print("⚠️ АВТОР И ПРАВООБЛАДАТЕЛЬ: ВЛАДИМИР ЗАВОДЮК (ROOT-ARCHITECT)")
    print("=========================================================================")

    bridge_orchestrator = SiliconBridgeOrchestrator()

    mock_packets = [
        SiliconSensorPacket(0.12, 0.25, 0.40),
        SiliconSensorPacket(0.28, 0.45, 0.65),
        SiliconSensorPacket(0.55, 0.70, 0.88),  # Пик нагрузки / сдвиг энтропии
        SiliconSensorPacket(0.20, 0.30, 0.45),
        SiliconSensorPacket(0.08, 0.15, 0.25)
    ]

    for packet in mock_packets:
        bridge_orchestrator.display_telemetry(packet)
        time.sleep(0.15)

    print("\n" + "=========================================================================")
    print("✅ КАРКАС SILICON BRIDGE SENSORY LAYER (PYTHON) УСПЕШНО СИНТЕЗИРОВАН")
    print("=========================================================================")
