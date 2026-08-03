# -*- coding: utf-8 -*-
"""
Chaos-Bound Rashemon Miner v3.0
Formula: 2 (Binary) + 3 (Ternary) = 5 (Pentary Attractor)
Purpose: Maximum Entropy Shroud / Anti-Analysis PoW
"""

import time
import math

# Сверхчувствительная пентарная матрица хаоса (5x5)
# Коэффициенты подобраны так, чтобы вызывать максимальное лавинообразное ветвление
CHAOS_PENTARY_MATRIX = [
    [ 2, -3,  1,  5, -2],
    [-5,  2,  3, -1,  4],
    [ 1, -4,  5,  2, -3],
    [ 3,  5, -2,  4,  1],
    [-2,  1, -4, -3,  5]
]

def binary_to_ternary_noise(binary_text):
    """Шаг [2 -> 3]: Превращает бинарную строку в троичный шумовой контур [-1, 0, 1]"""
    raw_bytes = binary_text.encode('utf-8')
    ternary_stream = []
    for b in raw_bytes:
        # Разбиваем байт на троичные составляющие (триты)
        val = b
        for _ in range(5):
            ternary_stream.append((val % 3) - 1)
            val //= 3
    return ternary_stream

def run_pentary_attractor(state_vector, matrix):
    """Шаг [3 -> 5]: Пропускает троичный шум через пятимерный аттрактор хаоса"""
    new_state = []
    for row in matrix:
        # Нелинейное псевдо-гиперболическое вращение вектора
        dot_product = sum(r * s for r, s in zip(row, state_vector))
        # Функция синуса создает нелинейный хаотический загиб пространства
        chaotic_fold = math.sin(dot_product) * 100
        # Возвращаем в дискретные триты [-1, 0, 1] через квантование фазы
        fractional, _ = math.modf(chaotic_fold)
        if fractional > 0.33:    new_state.append(1)
        elif fractional < -0.33: new_state.append(-1)
        else:                    new_state.append(0)
    return new_state

def render_visual_noise(vector):
    """Превращает пятимерный вектор в красивую текстовую шумовую завесу для консоли"""
    symbols = {1: "▓▒", 0: "░░", -1: "  "}
    return "".join(symbols[x] for x in vector)

def mine_chaos(block_data, difficulty_factor=13):
    """Основной цикл майнинга на динамическом хаосе"""
    print(f"[INIT] Запуск аттрактора хаоса для блока: '{block_data}'")
    
    # Переводим исходный бинарный текст в троичное шумовое поле
    noise_field = binary_to_ternary_noise(block_data)
    
    # Берем первые 5 тритов как стартовую точку в пространстве Лобачевского
    current_5d_point = noise_field[:5] if len(noise_field) >= 5 else [0, 1, -1, 0, 1]
    
    nonce = 0
    start_time = time.time()
    
    print("[ATTRACTOR] Генерация шумовой завесы запущена. Ищем точку стабилизации...")
    time.sleep(1)
    
    while True:
        nonce += 1
        
        # Эффект Расёмона: динамически меняем "угол обзора" матрицы хаоса на каждом шаге
        # Сдвигаем коэффициенты матрицы в зависимости от текущего шага и nonce
        dynamic_matrix = []
        for i, row in enumerate(CHAOS_PENTARY_MATRIX):
            shift = (nonce + i) % 3 - 1
            dynamic_matrix.append([elem + shift for elem in row])
            
        # Вращаем точку в 5D пространстве
        current_5d_point = run_pentary_attractor(current_5d_point, dynamic_matrix)
        
        # Выводим красивый неевклидов шум в консоль (балуемся визуалом)
        if nonce % 200 == 0:
            print(render_visual_noise(current_5d_point), end="", flush=True)
            
        # Критерий успешного майнинга (Детерминированный хаос):
        # Нам нужно поймать редкое состояние, когда псевдослучайное блуждание
        # выдает определенный математический резонанс с шагом майнинга
        chaos_resonance = sum(abs(x) for x in current_5d_point) * nonce
        
        if chaos_resonance % 1000000 == difficulty_factor:
            elapsed = time.time() - start_time
            print("\n\n[💥 RESONANCE FOUND] Система хаоса зафиксирована!")
            print(f"[NONCE] Итерация стабилизации: {nonce}")
            print(f"[FINAL STATE] Финальная 5D координата: {current_5d_point}")
            print(f"[DENSITY] Энтропия взломана за {elapsed:.4f} сек.")
            return nonce, current_5d_point

if __name__ == "__main__":
    tx = "System_Override_Protocol_2_3_5"
    # Фактор сложности задает фиксацию на волне хаоса
    mine_chaos(tx, difficulty_factor=77)
  
