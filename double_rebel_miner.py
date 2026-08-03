# -*- coding: utf-8 -*-
"""
Double Rebel Miner (2-Step PoW Engine)
Formula: 2 (Binary Brute-Force) -> 3 (Ternary Sieve) = 5D Block X Resonance
"""

import hashlib
import random
import time

# Наш неизменный Блок Х (Пентарная матрица 5x5)
BLOCK_X_MATRIX = [
    [ 1,  0, -1,  1,  0],
    [ 0,  1,  1, -1,  1],
    [-1,  1,  0,  1, -1],
    [ 1, -1,  1,  0,  1],
    [ 0,  1, -1,  1,  0]
]

def stage_1_binary_mine(block_data, target_zeros=2):
    """ЭТАП 2: Классический бинарный брутфорс (Ищем нули в начале хэша)"""
    print(f"[💥 ЭТАП 2: ДВОЙКИ] Запуск бинарного брутфорса...")
    prefix = "0" * target_zeros
    nonce = 0
    while True:
        nonce += 1
        text = f"{block_data}-{nonce}".encode('utf-8')
        h = hashlib.sha256(text).hexdigest()
        if h.startswith(prefix):
            print(f"[SUCCESS 2] Бинарный блок найден! Nonce: {nonce} | Hash: {h[:16]}...")
            return h, nonce

def hash_to_ternary_vector(hex_hash):
    """Промежуточный мост: перевод шестнадцатеричного хэша в троичный вектор 5D"""
    val = int(hex_hash, 16)
    vector = []
    for _ in range(5):
        vector.append((val % 3) - 1) # Приводим к [-1, 0, 1]
        val //= 3
    return vector

def stage_2_ternary_mine(binary_hash, difficulty=2):
    """ЭТАП 3: Троичное неевклидово сито через Блок Х"""
    print(f"\n[🌀 ЭТАП 3: ТРОЙКИ] Расщепление хэша в троичный вектор...")
    base_vector = hash_to_ternary_vector(binary_hash)
    print(f"[BASE 5D] Стартовая точка в пространстве Лобачевского: {base_vector}")
    
    t_nonce = 0
    while True:
        t_nonce += 1
        
        # Эффект Расёмона: подбираем троичный ключ модификации фазы
        rashemon_key = [random.choice([-1, 0, 1]) for _ in range(5)]
        
        # Сдвигаем базовый вектор ключом
        state = [(b + k) for b, k in zip(base_vector, rashemon_key)]
        state = [1 if x > 0 else (-1 if x < 0 else 0) for x in state] # Квантование
        
        # Прогоняем состояние через Блок Х (матрицу 5x5)
        transformed = []
        for row in BLOCK_X_MATRIX:
            dot_prod = sum(r * s for r, s in zip(row, state))
            # Нелинейное троичное квантование
            transformed.append(1 if dot_prod > 0 else (-1 if dot_prod < 0 else 0))
            
        # Условие победы: вектор должен схлопнуться в центральную зону симметрии (нули)
        if transformed.count(0) >= difficulty:
            print(f"[SUCCESS 3] Троичное сито пройдено! Итераций: {t_nonce}")
            print(f"[FINAL 5D] Резонансный вектор: {transformed}")
            print(f"[KEY] Геометрический ключ Расёмона: {rashemon_key}")
            return t_nonce, rashemon_key

if __name__ == "__main__":
    print("=== ЗАПУСК ДВОЙНОГО МАЙНИНГА (2 + 3 = 5) ===")
    start = time.time()
    
    block_payload = "TX_Zavodiuk_Final_Rebel_Block"
    
    # 1. Сначала двойки
    binary_hash, b_nonce = stage_1_binary_mine(block_payload, target_zeros=3)
    
    # 2. Потом тройки
    t_nonce, rashemon_key = stage_2_ternary_mine(binary_hash, difficulty=3)
    
    elapsed = time.time() - start
    print(f"\n=== БЛОК УСПЕШНО ДОБЫТ НА ОБЕИХ СКОРОСТЯХ! ===")
    print(f"Суммарное время хулиганства: {elapsed:.4f} сек.")
