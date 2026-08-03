# -*- coding: utf-8 -*-
"""
Pure Rashemon Effect Miner v2.0
Concept: Proof of Useful Compression (PoUC)
"""

import random
import time
import math

def text_to_matrix(text, size=4):
    """Преобразует текст блока в квадратную матрицу данных фиксированного размера"""
    bytes_data = text.encode('utf-8')
    matrix = []
    for i in range(size):
        row = []
        for j in range(size):
            idx = (i * size + j) % len(bytes_data)
            row.append(bytes_data[idx])
        matrix.append(row)
    return matrix

def calculate_entropy(vector):
    """Вычисляет приближенную энтропию вектора (критерий плотности сжатия)"""
    if not vector:
        return 0
    length = len(vector)
    counts = {}
    for val in vector:
        counts[val] = counts.get(val, 0) + 1
    
    entropy = 0.0
    for count in counts.values():
        p = count / length
        entropy -= p * math.log2(p)
    return entropy

def generate_perspective_matrix(size=4):
    """Генерирует случайную разреженную матрицу проекции (взгляд свидетеля)"""
    return [[random.choice([-1, 0, 1]) for _ in range(size)] for _ in range(size)]

def apply_projection(data_matrix, proj_matrix):
    """Линейная проекция данных через матрицу свидетеля"""
    size = len(data_matrix)
    result_vector = [0] * size
    for i in range(size):
        for j in range(size):
            result_vector[i] += data_matrix[i][j] * proj_matrix[j][i]
    return result_vector

def mine_rashemon(block_text, target_entropy=1.5):
    """Основной цикл майнинга через эффект Расёмона"""
    print(f"[INIT] Данные блока: '{block_text}'")
    
    # 1. Формируем исходное ядро данных
    data_core = text_to_matrix(block_text, size=4)
    print("[DATA] Исходная матрица данных сформирована.")
    
    nonce = 0
    start_time = time.time()
    
    while True:
        nonce += 1
        
        # 2. Эффект Расёмона: Генерируем 4 независимые точки зрения (матрицы)
        w1_bandit    = generate_perspective_matrix(size=4)
        w2_wife      = generate_perspective_matrix(size=4)
        w3_samurai   = generate_perspective_matrix(size=4)
        w4_woodcutter = generate_perspective_matrix(size=4)
        
        # 3. Собираем показания всех свидетелей (проекции)
        p1 = apply_projection(data_core, w1_bandit)
        p2 = apply_projection(data_core, w2_wife)
        p3 = apply_projection(data_core, w3_samurai)
        p4 = apply_projection(data_core, w4_woodcutter)
        
        # 4. Агрегируем проекции в единый сжатый контур консенсуса
        combined_consensus = [s1 + s2 + s3 + s4 for s1, s2, s3, s4 in zip(p1, p2, p3, p4)]
        
        # 5. Оцениваем полезность сжатия через энтропию
        current_entropy = calculate_entropy(combined_consensus)
        
        # Если энтропия ниже целевой — структуры данных идеально упакованы
        if current_entropy <= target_entropy and current_entropy > 0:
            elapsed = time.time() - start_time
            print("\n[SUCCESS] Блок упакован и добыт!")
            print(f"[NONCE] Найдено за {nonce} попыток совмещения матриц.")
            print(f"[ENTROPY] Целевая: <= {target_entropy} | Полученная: {current_entropy:.4f}")
            print(f"[CONSENSUS] Сжатый вектор блока: {combined_consensus}")
            print(f"[TIME] Время работы: {elapsed:.4f} секунд.")
            
            # Возвращаем ключи-матрицы, которые теперь служат доказательством архивации
            return {
                "nonce": nonce,
                "consensus_vector": combined_consensus,
                "witness_matrices": [w1_bandit, w2_wife, w3_samurai, w4_woodcutter]
            }
            
        if nonce % 50000 == 0:
            print(f"[MINING] Проверено {nonce} комбинаций свидетелей. Текущая энтропия: {current_entropy:.4f}")

if __name__ == "__main__":
    tx_data = "Zavodiuk_TX_Block_8888_Hash_Root"
    # Чем ниже target_entropy, тем жестче требования к сжатию (выше сложность майнинга)
    result = mine_rashemon(tx_data, target_entropy=1.2)
