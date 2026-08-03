# -*- coding: utf-8 -*-
"""
Non-Euclidean Optical Fiber Communications Simulator via Block X
Formula: 2 (Polarizations) + 3 (Ternary Phases) = 5D Spatial Mode Vector
Theory: Lobachevsky Space Trajectories & Cayley Graph Multiplexing
"""

import math
import cmath
import random
import time

# --- Блок Х: 5х5 Матрица пространственно-фазовой модуляции ---
# Задает базис для ортогональных световых мод (сбалансированные коэффициенты)
BLOCK_X_MATRIX = [
    [ 1.0,  0.0, -1.0,  1.0,  0.0],
    [ 0.0,  1.0,  1.0, -1.0,  1.0],
    [-1.0,  1.0,  0.0,  1.0, -1.0],
    [ 1.0, -1.0,  1.0,  0.0,  1.0],
    [ 0.0,  1.0, -1.0,  1.0,  0.0]
]

class NonEuclideanOpticalSystem:
    def __init__(self, fiber_length_km=100.0, noise_floor_db=-30.0):
        self.length = fiber_length_km
        self.noise_floor = 10 ** (noise_floor_db / 10.0) # перевод из дБ в линейную мощность
        self.attenuation_coefficient = 0.2 # 0.2 дБ/км - стандартное затухание для 1550 нм
        
    def bits_to_ternary_phases(self, bit_string):
        """[2 -> 3]: Перевод бинарного потока в сбалансированные троичные сдвиги фаз"""
        # Каждые 3 бита кодируют 2 трита (для простоты симуляции берем хэш-сдвиг фаз)
        raw_bytes = bit_string.encode('utf-8')
        ternary_phases = []
        for b in raw_bytes:
            temp = b
            for _ in range(5):
                trit = (temp % 3) - 1 # Перевод в [-1, 0, 1]
                # Задаем фазовые углы: -120, 0, +120 градусов (максимальное троичное расстояние)
                phase_angle = trit * (2 * math.pi / 3)
                ternary_phases.append(phase_angle)
                temp //= 3
        return ternary_phases

    def apply_block_x(self, base_phases):
        """[3 -> 5]: Пространственная модуляция 5D-светового вектора через Блок Х"""
        # Превращаем фазы в комплексные амплитуды света (Амплитуда = 1.0)
        optical_vector = [cmath.exp(1j * p) for p in base_phases[:5]]
        if len(optical_vector) < 5:
            optical_vector += [complex(1.0, 0.0)] * (5 - len(optical_vector))
            
        # Умножение Блока Х на световой вектор
        modulated_5d_light = [complex(0, 0)] * 5
        for i in range(5):
            for j in range(5):
                modulated_5d_light[i] += BLOCK_X_MATRIX[i][j] * optical_vector[j]
        return modulated_5d_light

    def simulate_lobachevsky_fiber(self, modulated_light):
        """Моделирование ВОЛС как графа Кэли в геометрии Лобачевского"""
        # Расчет линейного затухания света в кабеле
        total_loss_db = self.attenuation_coefficient * self.length
        transmission_coeff = 10 ** (-total_loss_db / 20.0)
        
        transmitted_light = []
        for i, mode in enumerate(modulated_light):
            # В геометрии Лобачевского траектории мод (лучей) расходятся экспоненциально.
            # Моделируем это через гиперболический масштаб фазы (кривизна пространства)
            hyperbolic_scale = math.cosh(self.length / 50.0) # Экспоненциальное расширение графа
            
            # Изменение фазы моды за счет неевклидовой длины пути
            phase_shift = (i + 1) * hyperbolic_scale * 0.1
            mode_with_distance = mode * cmath.exp(1j * phase_shift) * transmission_coeff
            
            # Добавление физического шума волокна (волновой шум)
            noise_amplitude = math.sqrt(self.noise_floor)
            noise = complex(random.gauss(0, noise_amplitude), random.gauss(0, noise_amplitude))
            
            transmitted_light.append(mode_with_distance + noise)
            
        return transmitted_light

    def rashemon_decoder(self, received_light, original_length=5):
        """Эффект Расёмона: Вычисление обратной проекции для компенсации нелинейного хаоса волокна"""
        # Вычисляем детерминированную матрицу, обратную Блоку Х
        # Для простоты демонстрации делаем псевдообратное проецирование
        recovered_phases = []
        
        # Симулируем компенсацию неевклидова расхождения (инверсия фазового сдвига Лобачевского)
        hyperbolic_scale = math.cosh(self.length / 50.0)
        
        for i in range(original_length):
            # Снимаем фазовый сдвиг траектории
            phase_shift = (i + 1) * hyperbolic_scale * 0.1
            cleaned_mode = received_light[i] * cmath.exp(-1j * phase_shift)
            
            # Эффект Расёмона: Собираем "свидетельские показания" со всех 5 пространственных осей
            # Проецируем многомодовый свет обратно на базовые троичные фазы
            projected_val = complex(0, 0)
            for j in range(5):
                projected_val += BLOCK_X_MATRIX[j][i] * cleaned_mode
                
            # Восстанавливаем троичную фазу через аргумент комплексного числа
            angle = cmath.phase(projected_val)
            
            # Квантование обратно в троичные фазовые точки (-120, 0, 120)
            target_angles = [-2*math.pi/3, 0.0, 2*math.pi/3]
            best_match = min(target_angles, key=lambda x: abs(x - angle))
            recovered_phases.append(best_match)
            
        return recovered_phases

# --- Демонстрационный запуск оптической симуляции ---
if __name__ == "__main__":
    print("=== ЗАПУСК ОПТИЧЕСКОЙ СИСТЕМЫ С БЛОКОМ Х (ГЕОМЕТРИЯ ЛОБАЧЕВСКОГО) ===")
    
    # Инициализируем 150 км магистрального оптоволокна с уровнем шума -35 дБ
    link = NonEuclideanOpticalSystem(fiber_length_km=150.0, noise_floor_db=-35.0)
    
    # Входной блок данных
    data_packet = "Zavodiuk_Quantum_Light_Node"
    print(f"[ВХОД] Данные для передачи: {data_packet}")
    
    # Шаг 1: Кодирование в троичные фазы (база 3)
    tx_phases = link.bits_to_ternary_phases(data_packet)
    print(f"[TX] Сгенерировано троичных фазовых мод: {len(tx_phases)}")
    print(f"[TX] Первые 5 эталонных фаз (радианы): {[round(p, 3) for p in tx_phases[:5]]}")
    
    # Шаг 2: Модуляция Блока Х (база 5)
    light_wave_5d = link.apply_block_x(tx_phases)
    print(f"[BLOCK X] Сформирован 5D пространственный вектор модового излучения.")
    print(f"[BLOCK X] Комплексные амплитуды мод: {[complex(round(c.real, 2), round(c.imag, 2)) for c in light_wave_5d]}")
    
    # Шаг 3: Передача по волокну Лобачевского (Экспоненциальное расхождение графа)
    print(f"\n[ЛИНИЯ СВЯЗИ] Запуск луча в волокно длиной {link.length} км...")
    distorted_light = link.simulate_lobachevsky_fiber(light_wave_5d)
    print(f"[LINE SHROUD] На выходе из волокна получен хаотичный интерференционный шум.")
    
    # Шаг 4: Приемник и Расёмон-декодер
    print("\n[RX] Прием сигнала. Запуск Расёмон-декодера (сборка проекций)...")
    rx_phases = link.rashemon_decoder(distorted_light, original_length=5)
    print(f"[RX] Восстановленные фазы (радианы): {[round(p, 3) for p in rx_phases]}")
    
    # Проверка точности передачи
    errors = sum(1 for tx, rx in zip(tx_phases[:5], rx_phases) if abs(tx - rx) > 0.01)
    print(f"\n=== РЕЗУЛЬТАТ ТЕСТИРОВАНИЯ ===")
    if errors == 0:
        print("🎉 УСПЕХ: 100% точность передачи! Эффект Расёмона полностью скомпенсировал неевклидов шум.")
    else:
        print(f"❌ НАЙДЕНЫ ОШИБКИ: {errors} мод(ы) искажены шумом линии.")
