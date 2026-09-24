#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# ==============================================================================
# TRINARY-PENTARY PIPELINE (Gödel/Kleene + Łukasiewicz)
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
README — TRINARY-PENTARY PIPELINE (единый демонстрационный файл)
================================================================================
Автор:  Vladimir Zavodiuk / Владимир Заводюк
Права:  All rights reserved. Copyright (c) 2026 Vladimir Zavodiuk.
Версия: 1.0 (демонстрационная, объединённая в один файл)

--------------------------------------------------------------------------------
1. ЧТО ЭТО
--------------------------------------------------------------------------------
Один файл, реализующий полный пайплайн обработки запроса через:
  ВХОД -> [BXOS: входной гейт] -> [ТРОИЧНЫЙ БЛОК] ->
          [СВЁРТКА ТРЁХ ТРИТОВ: Гёдель(min) ИЛИ Лукасевич(сильная)] ->
          [ПЕНТАРНЫЙ БЛОК] -> (опц.) [ТРОИЧНЫЕ МАТРИЦЫ] ->
          [BXOS: выходной гейт] -> ВЫХОД

Это НЕ дата-центр, НЕ замена LLM и НЕ ускоритель копирования данных.
Это демонстрационный пайплайн детерминированной обработки коротких
векторов (3 трита -> 5-значное состояние) с честной документацией
и обязательными самопроверками.

--------------------------------------------------------------------------------
2. ДВЕ ЛОГИКИ: ГЁДЕЛЬ/КЛИНИ vs ЛУКАСЕВИЧ
--------------------------------------------------------------------------------
В теории многозначных логик нет единственно верного определения
конъюнкции. Реализованы ДВА независимо изученных варианта (t-нормы):

  - Гёдель/Клини (GodelPipeline): AND = min.
    AND(0, 0) = 0  — нейтральный сигнал остаётся нейтральным,
    не эскалирует.

  - Лукасевич (FullPipeline): AND выведена из двух законов
    (отрицание + импликация).
    AND(0, 0) = -1 — неопределённость трактуется как повод
    усомниться сильнее.

Какой нужен — зависит от задачи, а не от того, какой "лучше".
Оба варианта доступны через общий API (process) и могут сравниваться
на одних и тех же входных данных.

Важное свойство: сильная конъюнкция Лукасевича НЕ гарантированно
ассоциативна на всех входных данных — это проверяется в self_check
явно, а не предполагается. Гёдель/Клини (min) ассоциативен
математически, проверка для него — контрольная.

--------------------------------------------------------------------------------
3. ЧТО НАЙДЕНО И ИСПРАВЛЕНО (честный раздел)
--------------------------------------------------------------------------------
При объединении нескольких исходных файлов в один был найден
реальный баг, который прошёл синтаксическую проверку, но давал
НЕВЕРНЫЙ результат молча:

  В исходном full_pipeline.py импорт был оформлен как
      from brusentsov_trits import fold_and as fold_and_scalar
  При слиянии строка импорта убирается, а имя fold_and_scalar
  УЖЕ существовало в файле как отдельная функция в пентарном блоке
  (другая логика, другой домен: k=2 вместо k=1). Код Лукасевича
  стал вызывать чужую функцию с тем же именем, без единой ошибки
  выполнения — просто с другим числом на выходе.

  Обнаружено только потому, что результат сравнили с оригиналом,
  а не понадеялись на успешный запуск.

Исправлено: вызовы приведены к настоящему имени fold_and.
Дополнительно устранены конфликты имён:
  - fold_and_scalar/fold_and_batch/fold_or_scalar/fold_or_batch
    у Гёделя переименованы в godel_fold_and_scalar и т.д., чтобы
    не совпадать с одноимёнными (но другими по логике) функциями
    пентарного блока.

Также в обязательную самопроверку добавлена явная проверка
ассоциативности Лукасевича на всех 27 тройках {-1, 0, 1} — то, чего
не было раньше.

--------------------------------------------------------------------------------
4. ЧТО УДАЛЕНО И ПОЧЕМУ
--------------------------------------------------------------------------------
В прошлых версиях присутствовали компоненты, которые были декоративны
или работали не так, как заявлено. Все они удалены:

  - LearningGuard в троичном блоке. Его функция (ограничение роста,
    аварийный veto/resume) по смыслу — функция гейта безопасности
    (BXOS), а не троичного Fast-Path. В прежнем виде он к тому же не
    работал как заявлено: get_best_threshold усреднял сам себя и не
    учился на входных данных.

  - Самообучаемый порог quantize. Пороги теперь фиксированные
    параметры блока (можно передать явно при создании).

  - sign_normalization, dominance_map, stability_map,
    transition_velocity, stabilization в троичных матрицах. Все пять
    алгебраически равны identity(matrix) на домене {-1, 0, 1}: "взять
    знак числа, которое и так уже -1/0/1" ничего не меняет.

  - conflict_index — дубликат diagonal_projection (идентичный код).

  - transposed_dominance — падал на неквадратных матрицах и дублировал
    transpose() на квадратных.

  - recover_from_* — были документированы как заглушки
    (return self.copy()), несущей функции не было.

  - Гиперболическая геометрия в BXOS v1.2: доказано на числах, что
    метрики квантуются в трит ДО применения геометрии, из-за чего
    3 из 5 разных тестовых входов схлопнулись в одну точку (0,0,0).

  - time.sleep(0.3) на тик планировщика в BXOS — противоречил
    заявленной "нулевой задержке".

  - fold_and_via_trait_table — не делала того, что обещала
    (табличная свёртка). Переименована в trait_and_3trits и честно
    документирована как поразрядный AND для 3 тритов, не свёртка.

  - Таблицы упаковки (trait/pentait) для Гёделя/Клини НЕ применяются:
    замерено, что они проигрывают скаляру в 7.48x на одиночных
    вызовах. Для Лукасевича таблица тоже не используется по умолчанию:
    её выигрыш — только при батч-обработке миллионов значений разом,
    не при 1 запрос = 1 свёртка.

--------------------------------------------------------------------------------
5. ЧТО ЭТОТ ФАЙЛ НЕ ДЕЛАЕТ (важно)
--------------------------------------------------------------------------------
  - НЕ заменяет LLM. Это детерминированная логическая свёртка для
    коротких векторов, а не языковая модель.
  - НЕ ускоряет дата-центр и НЕ экономит электроэнергию. Работает в
    слое, где энергия почти не тратится (короткие int8-векторы).
  - НЕ конкурирует с numpy, pandas и т.п. Использует их.
  - НЕ решает задачу классификации/распознавания в общем случае.
    Решает узкую задачу: свести 3 трита в одно 5-значное состояние
    и проверить его через гейт.
  - НЕ содержит физической энергии. Метрика signal_magnitude —
    это sum(|x|) / 10, условная амплитуда сигнала.

--------------------------------------------------------------------------------
6. ЗАПУСК
--------------------------------------------------------------------------------
  pip install numpy
  python godel_lukasiewicz_pipeline.py              # демо + самопроверки
  python godel_lukasiewicz_pipeline.py --readme      # только README
  python godel_lukasiewicz_pipeline.py --benchmark   # + бенчмарк
  python godel_lukasiewicz_pipeline.py --json        # вывод в JSON
  python godel_lukasiewicz_pipeline.py --help        # справка

--------------------------------------------------------------------------------
7. МЕТРИКИ: ЧТО ИЗМЕРЯЕМ, ЧТО НЕ ИЗМЕРЯЕМ
--------------------------------------------------------------------------------
Измеряем:
  - время вызова process() (мкс),
  - решений в секунду (decisions/sec),
  - число расхождений scalar vs batch путей в self_check,
  - число нарушений ассоциативности Лукасевича в self_check.

НЕ измеряем и НЕ заявляем:
  - GOPS/FLOPS (это hardware-метрики, к данному коду не относятся),
  - "экономию токенов" (в файле нет токенов),
  - "экономию энергии" (нет физической энергии),
  - "ускорение дата-центра" (файл не работает с данными дата-центра).

--------------------------------------------------------------------------------
8. ИЗВЕСТНЫЕ ОГРАНИЧЕНИЯ
--------------------------------------------------------------------------------
  - Лукасевич: сильная конъюнкция может быть НЕ ассоциативна на
    отдельных входных данных. Проверка ассоциативности в self_check
    покажет, есть ли нарушения. Если есть — fold_and для Лукасевича
    не коммутативна в строгом смысле и порядок применения важен.
  - Матрицы: операции add/multiply используют L3-алгебру с
    клиппингом в {-1, 0, 1}. Это НЕ линейная алгебра, а насыщающаяся.
  - determinat реализован для 1x1, 2x2, 3x3.
  - AuditChain ограничен 500 записями (без утечки памяти).
  - TRAIT_AND_TABLE строится lazy (при первом обращении, не при
    импорте) — чтобы import был быстрым.
  - Хэши в AuditChain кэшируются через cached_property.

--------------------------------------------------------------------------------
9. ЛИЦЕНЗИЯ И АВТОРСТВО
--------------------------------------------------------------------------------
Author: Vladimir Zavodiuk / Владимир Заводюк
Copyright (c) 2026 Vladimir Zavodiuk
All rights reserved.

Использование, копирование, модификация и распространение допускаются
только с письменного разрешения автора.

Pull request'ы с тестами, бенчмарками и исправлениями приветствуются;
отправляя PR, вы соглашаетесь с условиями текущей лицензии, авторство
остаётся за Vladimir Zavodiuk.

================================================================================
"""

import sys
import os
import time
import json
import math
import hashlib
import argparse
import numpy as np
from enum import Enum
from functools import cached_property
from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple, Sequence, Union
from itertools import product as iproduct


# ============================================================================
# ТРОИЧНЫЙ БЛОК (Fast-Path)
# ============================================================================

def quantize(value: float, low: float = 0.3, high: float = 0.7) -> int:
    """Число -> трит {-1, 0, +1}. Границы включительны."""
    if value >= high:
        return 1
    if value <= low:
        return -1
    return 0


def normalize_sign(x: float) -> int:
    if x > 0:
        return 1
    if x < 0:
        return -1
    return 0


def ternary_fold(vector: Tuple[int, ...]) -> int:
    """Свёртка вектора тритов в один трит по знаку суммы."""
    return normalize_sign(sum(vector))


def ternary_diff(vec_a: Tuple[int, ...], vec_b: Tuple[int, ...]) -> Tuple[int, ...]:
    if len(vec_a) != len(vec_b):
        raise ValueError(f"Размерности не совпадают: {len(vec_a)} != {len(vec_b)}")
    return tuple(a - b for a, b in zip(vec_a, vec_b))


def rashomon_analyze(interpretations: Dict[str, float],
                     margin_threshold: float = 0.15,
                     min_score: float = 0.3) -> Dict:
    """Входной триаж: явный брак (-1) / неопределённость (0) / уверенный лидер (1)."""
    if not interpretations:
        return {'state': -1, 'top': None, 'margin': 0.0, 'top_score': 0.0,
                'reason': 'Нет интерпретаций'}

    sorted_items = sorted(interpretations.items(), key=lambda x: x[1], reverse=True)
    top_label, top_score = sorted_items[0]
    margin = top_score - sorted_items[1][1] if len(sorted_items) > 1 else top_score

    if top_score < min_score:
        state, reason = -1, f"Низкая оценка: {top_score:.2f}"
    elif margin >= margin_threshold:
        state, reason = 1, f"Устойчивое ядро: отрыв {margin:.2f}"
    else:
        state, reason = 0, f"Неопределённость: отрыв {margin:.2f}"

    return {
        'state': state, 'top': top_label, 'margin': margin,
        'top_score': top_score, 'reason': reason,
        'all_scores': dict(sorted_items),
    }


def efficiency(value: float, cost: float) -> float:
    """Защищённый от деления на ноль расчёт эффективности."""
    if cost == 0.0:
        cost = 0.001
    total = value + cost
    return value / total if total > 0 else 0.0


@dataclass
class TernaryResult:
    query: str
    l3_in: int
    l3_in_reason: str
    l3_out: int
    best_candidate: Optional[str]
    efficiency: float
    efficiency_trit: int
    final_decision: str  # "REJECT" | "CLARIFY" | "PASS"
    rollback_needed: bool


class TernaryBlock:
    """Троичный блок — минимальное рабочее ядро Fast-Path слоя.
    Без самообучения и без функций безопасности (это к BXOS)."""

    def __init__(self, low: float = 0.3, high: float = 0.7,
                 margin_threshold: float = 0.15, min_score: float = 0.3,
                 max_history: int = 500):
        self.low = low
        self.high = high
        self.margin_threshold = margin_threshold
        self.min_score = min_score
        self.max_history = max_history
        self.history: List[Dict] = []

    def process(self, query: str, interpretations: Dict[str, float]) -> TernaryResult:
        triage = rashomon_analyze(interpretations, self.margin_threshold, self.min_score)
        l3_in = triage['state']

        if l3_in == -1:
            result = TernaryResult(
                query=query, l3_in=-1, l3_in_reason=triage['reason'],
                l3_out=-1, best_candidate=None, efficiency=0.0,
                efficiency_trit=-1, final_decision="REJECT",
                rollback_needed=True,
            )
            self._log(result)
            return result

        best_name, best_score = None, -1.0
        best_eff = 0.0
        for name, score in interpretations.items():
            eff = efficiency(score, 1.0 - score)
            if eff > best_eff:
                best_eff, best_name, best_score = eff, name, score

        l3_out = quantize(best_score, self.low, self.high)
        eff_trit = quantize(best_eff, self.low, self.high)

        final_decision = "PASS" if l3_out == 1 else ("CLARIFY" if l3_out == 0 else "REJECT")

        result = TernaryResult(
            query=query, l3_in=l3_in, l3_in_reason=triage['reason'],
            l3_out=l3_out, best_candidate=best_name, efficiency=best_eff,
            efficiency_trit=eff_trit, final_decision=final_decision,
            rollback_needed=(l3_out == -1),
        )
        self._log(result)
        return result

    def _log(self, result: TernaryResult):
        self.history.append({'time': time.time(), 'query': result.query,
                             'decision': result.final_decision})
        if len(self.history) > self.max_history:
            self.history.pop(0)


# ============================================================================
# БРУСЕНЦОВ / ЛУКАСЕВИЧ (троичная логика, 2 закона)
# ============================================================================

Trit = int  # -1, 0, 1


def _check_trit(t: Trit) -> Trit:
    if t not in (-1, 0, 1):
        raise ValueError(f"Не трит: {t} (допустимо -1, 0, 1)")
    return t


# ---- 1. ДВА ЗАКОНА: ОТРИЦАНИЕ И ИМПЛИКАЦИЯ ----

def trit_not(t: Trit) -> Trit:
    """Отрицание: -t. F<->T меняются местами, N (0) остаётся собой."""
    return -_check_trit(t)


_IMPL_TABLE = {
    (-1, -1): 1, (-1, 0): 1, (-1, 1): 1,
    (0, -1): 0, (0, 0): 1, (0, 1): 1,
    (1, -1): -1, (1, 0): 0, (1, 1): 1,
}


def trit_impl(a: Trit, b: Trit) -> Trit:
    """Импликация a -> b."""
    return _IMPL_TABLE[(_check_trit(a), _check_trit(b))]


# ---- 2. ВЫВЕДЕННЫЕ ОПЕРАЦИИ (через отрицание + импликацию) ----

def trit_and(a: Trit, b: Trit) -> Trit:
    """a AND b := NOT(a -> NOT(b)). Сильная конъюнкция Лукасевича."""
    return trit_not(trit_impl(a, trit_not(b)))


def trit_or(a: Trit, b: Trit) -> Trit:
    """a OR b := (NOT a) -> b."""
    return trit_impl(trit_not(a), b)


def fold_and(vector: Tuple[Trit, ...]) -> Trit:
    """Свёртка вектора тритов через AND (цепочкой)."""
    if not vector:
        return 1  # пустая конъюнкция = истина (нейтральный элемент)
    result = vector[0]
    for t in vector[1:]:
        result = trit_and(result, t)
    return result


def fold_or(vector: Tuple[Trit, ...]) -> Trit:
    """Свёртка вектора тритов через OR (цепочкой)."""
    if not vector:
        return -1  # пустая дизъюнкция = ложь (нейтральный элемент)
    result = vector[0]
    for t in vector[1:]:
        result = trit_or(result, t)
    return result


# ---- 3. СБАЛАНСИРОВАННАЯ ТРОИЧНАЯ АРИФМЕТИКА ----

TRAIT_SIZE = 6  # трайт = 6 тритов (как в "Сетуни-70"), диапазон -364..+364
TRAIT_WIDTH = 6
TRAIT_STATES = 3 ** TRAIT_WIDTH  # 729
TRAIT_OFFSET = (TRAIT_STATES - 1) // 2  # 364


def to_balanced_ternary(n: int, width: int = TRAIT_SIZE) -> List[Trit]:
    """Целое число -> список тритов (младший разряд первым)."""
    max_val = (3 ** width - 1) // 2
    if abs(n) > max_val:
        raise ValueError(f"Число {n} не помещается в {width} тритов (диапазон ±{max_val})")
    trits = []
    v = n
    for _ in range(width):
        v, rem = divmod(v, 3)
        if rem == 2:
            rem = -1
            v += 1
        trits.append(rem)
    return trits


def from_balanced_ternary(trits: List[Trit]) -> int:
    """Список тритов (младший разряд первым) -> целое число."""
    value = 0
    for i, t in enumerate(trits):
        value += _check_trit(t) * (3 ** i)
    return value


def trit_add_with_carry(a: Trit, b: Trit, carry_in: Trit = 0) -> Tuple[Trit, Trit]:
    """Сложение двух тритов с переносом. Возвращает (результат, перенос).

    Диапазон total = a + b + carry_in = [-3, 3]. Циклы while выполняются
    максимум один раз (перестраховка для ясности).
    """
    total = _check_trit(a) + _check_trit(b) + _check_trit(carry_in)
    carry_out = 0
    if total > 1:
        total -= 3
        carry_out = 1
    elif total < -1:
        total += 3
        carry_out = -1
    return total, carry_out


def add_balanced_ternary(vec_a: List[Trit], vec_b: List[Trit]) -> List[Trit]:
    """Поразрядное сложение двух троичных чисел (младший разряд первым)."""
    width = max(len(vec_a), len(vec_b))
    a = vec_a + [0] * (width - len(vec_a))
    b = vec_b + [0] * (width - len(vec_b))
    result = []
    carry = 0
    for i in range(width):
        digit, carry = trit_add_with_carry(a[i], b[i], carry)
        result.append(digit)
    if carry != 0:
        result.append(carry)
    return result


# ============================================================================
# ТРАЙТ-ТАБЛИЦА (поразрядный AND, не свёртка)
# ============================================================================

_IMPL3 = {(-1, -1): 1, (-1, 0): 1, (-1, 1): 1,
          (0, -1): 0, (0, 0): 1, (0, 1): 1,
          (1, -1): -1, (1, 0): 0, (1, 1): 1}


def _trit_and_scalar(a: int, b: int) -> int:
    return -_IMPL3[(a, -b)]


def _decode_trait(n: int) -> List[int]:
    trits = []
    v = n
    for _ in range(TRAIT_WIDTH):
        v, rem = divmod(v, 3)
        if rem == 2:
            rem = -1
            v += 1
        trits.append(rem)
    return trits


def _encode_trait(trits) -> int:
    return sum(t * (3 ** i) for i, t in enumerate(trits))


_TRAIT_AND_TABLE_CACHE: Optional[np.ndarray] = None


def _get_trait_and_table() -> np.ndarray:
    """Lazy-инициализация таблицы (~1-2 сек). Вызывается при первом
    обращении, не при импорте модуля."""
    global _TRAIT_AND_TABLE_CACHE
    if _TRAIT_AND_TABLE_CACHE is None:
        table = np.zeros((TRAIT_STATES, TRAIT_STATES), dtype=np.int16)
        for a_enc in range(-TRAIT_OFFSET, TRAIT_OFFSET + 1):
            a_trits = _decode_trait(a_enc)
            for b_enc in range(-TRAIT_OFFSET, TRAIT_OFFSET + 1):
                b_trits = _decode_trait(b_enc)
                result = [_trit_and_scalar(x, y) for x, y in zip(a_trits, b_trits)]
                table[a_enc + TRAIT_OFFSET, b_enc + TRAIT_OFFSET] = _encode_trait(result)
        _TRAIT_AND_TABLE_CACHE = table
    return _TRAIT_AND_TABLE_CACHE


def trait_and(a, b):
    """Поразрядный AND двух трайтов (значения -364..364).

    ВАЖНО: это НЕ свёртка. Это поразрядная операция над 6 тритами
    внутри трайта. Для свёртки используйте fold_and.
    """
    a_arr = np.asarray(a, dtype=np.int64)
    b_arr = np.asarray(b, dtype=np.int64)
    if np.any(np.abs(a_arr) > TRAIT_OFFSET) or np.any(np.abs(b_arr) > TRAIT_OFFSET):
        raise ValueError(f"Значения должны быть в [-{TRAIT_OFFSET}, {TRAIT_OFFSET}]")
    table = _get_trait_and_table()
    return table[a_arr + TRAIT_OFFSET, b_arr + TRAIT_OFFSET]


def encode_trait(trits) -> int:
    return _encode_trait(trits)


def decode_trait(n: int) -> List[int]:
    return _decode_trait(n)


def trait_and_3trits(trits) -> int:
    """Поразрядный AND для 3 тритов через трайт-таблицу.

    ВАЖНО: это НЕ свёртка трёх тритов в один. Функция возвращает
    поразрядный AND (три результата внутри трайта), а не одно число.
    Название переименовано из fold_and_via_trait_table, потому что
    прежнее имя вводило в заблуждение.
    """
    if len(trits) > TRAIT_WIDTH:
        raise ValueError(f"Максимум {TRAIT_WIDTH} тритов")
    values = list(trits) + [1] * (TRAIT_WIDTH - len(trits))
    a = encode_trait(values)
    b = encode_trait([1] * TRAIT_WIDTH)
    return int(trait_and(a, b))


# ============================================================================
# ЛОГИКА ГЁДЕЛЯ/КЛИНИ (min/max)
# ============================================================================

ArrayLike = Union[int, np.ndarray]


def godel_not(a: ArrayLike) -> ArrayLike:
    return -np.asarray(a) if isinstance(a, np.ndarray) else -a


def godel_and(a: ArrayLike, b: ArrayLike) -> ArrayLike:
    """AND = min. Работает для любого домена без изменений —
    ассоциативность математически гарантирована."""
    return np.minimum(a, b)


def godel_or(a: ArrayLike, b: ArrayLike) -> ArrayLike:
    return np.maximum(a, b)


def godel_fold_and_scalar(values: Sequence[int]) -> int:
    """Свёртка одного вектора через AND (min). Самый быстрый путь
    для одиночных вызовов — упаковка не нужна."""
    if not values:
        raise ValueError("Пустая последовательность")
    result = values[0]
    for v in values[1:]:
        result = min(result, v)
    return result


def godel_fold_or_scalar(values: Sequence[int]) -> int:
    if not values:
        raise ValueError("Пустая последовательность")
    result = values[0]
    for v in values[1:]:
        result = max(result, v)
    return result


def godel_fold_and_batch(matrix: np.ndarray) -> np.ndarray:
    """Свёртка через AND сразу для M векторов (матрица MxK)."""
    return np.minimum.reduce(matrix, axis=1)


def godel_fold_or_batch(matrix: np.ndarray) -> np.ndarray:
    return np.maximum.reduce(matrix, axis=1)


# ============================================================================
# ПЕНТАРНЫЙ БЛОК
# ============================================================================

K = 2  # параметр домена пентарной логики: значения -2..2


class PentarnyState(Enum):
    """-2: сильное "нет"; -1: "нет"; 0: неопределённость;
    1: "да"; 2: сильное "да"."""
    STRONG_FALSE = -2
    FALSE = -1
    UNDEFINED = 0
    TRUE = 1
    STRONG_TRUE = 2

    def is_positive(self) -> bool:
        return self.value > 0

    def is_negative(self) -> bool:
        return self.value < 0

    def is_undefined(self) -> bool:
        return self.value == 0

    def strength(self) -> float:
        return abs(self.value) / 2.0


@dataclass
class PentarnyStateData:
    value: PentarnyState
    confidence: float
    timestamp: float
    metadata: Dict[str, Any]

    def to_dict(self) -> Dict:
        return {
            'value': self.value.value, 'label': self.value.name,
            'confidence': self.confidence, 'timestamp': self.timestamp,
            'metadata': self.metadata,
        }


Penta = np.ndarray


def _check_penta(arr: Penta) -> Penta:
    if not np.all(np.isin(arr, [-2, -1, 0, 1, 2])):
        raise ValueError("Массив содержит значения вне {-2,-1,0,1,2}")
    return arr


def penta_not(a: Penta) -> Penta:
    return -_check_penta(a)


def penta_impl(a: Penta, b: Penta) -> Penta:
    """impl(a,b) = min(K, K - a + b). Закрытая формула, таблица не нужна."""
    a, b = _check_penta(a), _check_penta(b)
    if a.shape != b.shape:
        raise ValueError(f"Размеры не совпадают: {a.shape} != {b.shape}")
    return np.minimum(K, K - a + b).astype(np.int8)


def penta_and(a: Penta, b: Penta) -> Penta:
    """AND(a,b) = NOT(a -> NOT(b)) = max(-K, a+b-K). Сильная конъюнкция."""
    return penta_not(penta_impl(a, penta_not(b)))


def penta_or(a: Penta, b: Penta) -> Penta:
    """OR(a,b) = (NOT a) -> b = min(K, a+b+K)."""
    return penta_impl(penta_not(a), b)


def penta_and_scalar(a: int, b: int) -> int:
    return int(penta_and(np.array([a], dtype=np.int8), np.array([b], dtype=np.int8))[0])


def penta_or_scalar(a: int, b: int) -> int:
    return int(penta_or(np.array([a], dtype=np.int8), np.array([b], dtype=np.int8))[0])


def _tree_reduce_batch(matrix: Penta, op, neutral: int) -> np.ndarray:
    """Векторизованная свёртка M независимых векторов (MxK) в M чисел
    деревом (log2(K) шагов)."""
    m = _check_penta(matrix).copy()
    while m.shape[1] > 1:
        k = m.shape[1]
        if k % 2 == 1:
            m = np.concatenate([m, np.full((m.shape[0], 1), neutral, dtype=np.int8)], axis=1)
        left, right = m[:, 0::2], m[:, 1::2]
        m = op(left, right)
    return m[:, 0]


def fold_and_batch(matrix: Penta) -> np.ndarray:
    """Нейтральный элемент AND = +2."""
    return _tree_reduce_batch(matrix, penta_and, neutral=2)


def fold_or_batch(matrix: Penta) -> np.ndarray:
    """Нейтральный элемент OR = -2."""
    return _tree_reduce_batch(matrix, penta_or, neutral=-2)


def fold_and_scalar(values: List[int]) -> int:
    """Свёртка одного вектора через AND (для удобства без NumPy)."""
    if not values:
        return 2
    result = values[0]
    for v in values[1:]:
        result = penta_and_scalar(result, v)
    return result


def fold_or_scalar(values: List[int]) -> int:
    if not values:
        return -2
    result = values[0]
    for v in values[1:]:
        result = penta_or_scalar(result, v)
    return result


# ---- L5 ----

class L5Layer(Enum):
    INTENT = "intent"
    GOAL = "goal"
    ONTOLOGY = "ontology"
    CONTEXT = "context"
    ACTION = "action"


@dataclass
class L5State:
    layer: L5Layer
    state: PentarnyStateData
    transition_magnitude: float

    def to_dict(self) -> Dict:
        return {'layer': self.layer.value, 'state': self.state.to_dict(),
                'transition_magnitude': self.transition_magnitude}


class L5Engine:
    """Управление пятью слоями пентарной логики. История ограничена.

    ВНИМАНИЕ: поле smoothed_magnitude — условная метрика амплитуды
    сигнала (экспоненциально сглаженное abs(value)/2), НЕ физическая
    энергия.
    """

    def __init__(self, max_history: int = 500):
        self.states: Dict[L5Layer, Optional[PentarnyStateData]] = {l: None for l in L5Layer}
        self.history: List[L5State] = []
        self.smoothed_magnitude = 0.5
        self.max_history = max_history

    def set_state(self, layer: L5Layer, value: int, confidence: float = 0.5,
                  metadata: Optional[Dict[str, Any]] = None) -> PentarnyStateData:
        if value not in (-2, -1, 0, 1, 2):
            raise ValueError("Значение должно быть -2, -1, 0, 1 или 2")

        p_state = PentarnyState(value)
        state_data = PentarnyStateData(value=p_state, confidence=confidence,
                                       timestamp=time.time(),
                                       metadata=metadata or {})
        self.states[layer] = state_data

        transition_magnitude = abs(value) / 2.0
        self.history.append(L5State(layer, state_data, transition_magnitude))
        if len(self.history) > self.max_history:
            self.history.pop(0)
        self.smoothed_magnitude = (self.smoothed_magnitude + transition_magnitude) / 2.0

        return state_data

    def get_state(self, layer: L5Layer) -> Optional[PentarnyStateData]:
        return self.states.get(layer)

    def evaluate(self) -> Dict[str, Any]:
        result = {}
        total_magnitude = 0.0
        count = 0
        for layer in L5Layer:
            state = self.states.get(layer)
            if state:
                result[layer.value] = state.to_dict()
                total_magnitude += abs(state.value.value) / 2.0
                count += 1
            else:
                result[layer.value] = None

        avg_magnitude = total_magnitude / count if count > 0 else 0.0

        values = [s.value.value for s in self.states.values() if s is not None]
        if values:
            avg_val = sum(values) / len(values)
            consistency = "positive" if avg_val > 0.5 else (
                "negative" if avg_val < -0.5 else "neutral")
        else:
            consistency = "undefined"

        return {
            'states': result,
            'avg_magnitude': avg_magnitude,           # было avg_energy
            'consistency': consistency,
            'smoothed_magnitude': self.smoothed_magnitude,  # было system_energy
            'history_len': len(self.history),
        }


# ---- RASHOMON (переименован, 4 перспективы) ----

class PentarnyRashomonAudit:
    """Многоракурсный отчёт (Rashomon) — 4 перспективы от одного состояния.

    ВНИМАНИЕ: все четыре формулы читают ОДНО состояние под разными
    углами — это не независимые эксперты, а компактный отчёт.
    Переименован в PentarnyRashomonAudit, чтобы отличать от 5-перспективной
    версии в harmony_datacenter_node.py.
    """

    def __init__(self):
        self.perspectives = {
            'technical': self._technical,
            'legal': self._legal,
            'business': self._business,
            'strategic': self._strategic,
        }

    def _technical(self, state: PentarnyStateData) -> Dict:
        return {'score': state.value.value / 2.0, 'stability': state.confidence,
                'complexity': abs(state.value.value) / 2.0}

    def _legal(self, state: PentarnyStateData) -> Dict:
        return {'score': 0.5 if state.value.is_positive() else -0.5,
                'risk': 1.0 - state.confidence,
                'compliance': 1.0 if state.value != PentarnyState.UNDEFINED else 0.0}

    def _business(self, state: PentarnyStateData) -> Dict:
        return {'score': state.value.value / 2.0 * state.confidence,
                'roi': state.confidence * 0.8,
                'market_fit': abs(state.value.value) / 2.0}

    def _strategic(self, state: PentarnyStateData) -> Dict:
        return {'score': state.value.value / 2.0,
                'urgency': 1.0 - state.confidence,
                'recommendation': self._get_recommendation(state)}

    def _get_recommendation(self, state: PentarnyStateData) -> str:
        return {
            PentarnyState.STRONG_TRUE: "Активно действовать",
            PentarnyState.TRUE: "Продолжать",
            PentarnyState.UNDEFINED: "Требуется анализ",
            PentarnyState.FALSE: "Пересмотреть",
            PentarnyState.STRONG_FALSE: "Немедленно остановиться",
        }[state.value]

    def analyze(self, state: PentarnyStateData) -> Dict[str, Dict]:
        return {name: func(state) for name, func in self.perspectives.items()}

    def to_dict(self, state: PentarnyStateData) -> Dict:
        return {'state': state.to_dict(), 'analysis': self.analyze(state)}


# ---- AuditChain с cached_property для хэшей ----

@dataclass
class ChainRecord:
    index: int
    timestamp: float
    prev_hash: str
    state: PentarnyStateData
    payload: Dict[str, Any]

    @cached_property
    def hash(self) -> str:
        data = f"{self.index}{self.timestamp}{self.prev_hash}{json.dumps(self.payload, sort_keys=True)}"
        return hashlib.sha256(data.encode()).hexdigest()

    def to_dict(self) -> Dict:
        return {'index': self.index, 'timestamp': self.timestamp,
                'prev_hash': self.prev_hash[:16] + '...' if len(self.prev_hash) > 16 else self.prev_hash,
                'state': self.state.to_dict(), 'payload': self.payload,
                'hash': self.hash[:16] + '...'}


class AuditChain:
    def __init__(self, max_length: int = 500):
        self.records: List[ChainRecord] = []
        self.max_length = max_length

    def add(self, state: PentarnyStateData, payload: Dict[str, Any]) -> ChainRecord:
        record = ChainRecord(
            index=len(self.records), timestamp=time.time(),
            prev_hash=self.records[-1].hash if self.records else 'GENESIS',
            state=state, payload=payload,
        )
        self.records.append(record)
        if len(self.records) > self.max_length:
            self.records.pop(0)
        return record

    def verify(self) -> Tuple[bool, int]:
        for i, record in enumerate(self.records):
            if i > 0 and record.prev_hash != self.records[i - 1].hash:
                return False, i
        return True, -1

    def to_dict(self) -> Dict:
        verified, error_idx = self.verify()
        return {'length': len(self.records), 'verified': verified,
                'error_index': error_idx if not verified else None,
                'records': [r.to_dict() for r in self.records[-5:]]}


# ---- Мостик из троичного слоя ----

# Границы вынесены из функции в константы
PENTARY_BOUNDS = (
    (0.1, PentarnyState.STRONG_FALSE),
    (0.35, PentarnyState.FALSE),
    (0.65, PentarnyState.UNDEFINED),
    (0.9, PentarnyState.TRUE),
)


def value_to_pentarny(score: float) -> PentarnyState:
    """Непрерывный скор [0,1] -> 5 состояний."""
    for threshold, state in PENTARY_BOUNDS:
        if score <= threshold:
            return state
    return PentarnyState.STRONG_TRUE


# ---- Пентарный блок ----

class PentaryBlock:
    """Пентарный блок пайплайна: L5 + логика Брусенцова (2 закона) +
    Rashomon-отчёт + AuditChain."""

    def __init__(self):
        self.l5 = L5Engine()
        self.rashomon = PentarnyRashomonAudit()
        self.chain = AuditChain()
        self.current_state: Optional[PentarnyStateData] = None

    def set_state(self, value: int, confidence: float = 0.5,
                  metadata: Optional[Dict[str, Any]] = None) -> PentarnyStateData:
        if value not in (-2, -1, 0, 1, 2):
            raise ValueError("Значение должно быть -2, -1, 0, 1 или 2")
        self.current_state = PentarnyStateData(
            value=PentarnyState(value), confidence=confidence,
            timestamp=time.time(), metadata=metadata or {},
        )
        self.chain.add(self.current_state,
                       {'type': 'state_set', 'value': value, 'confidence': confidence})
        return self.current_state

    def receive_from_ternary(self, best_candidate: Optional[str], score: float,
                             confidence: float = 0.5) -> PentarnyStateData:
        """Точка стыковки с троичным блоком."""
        p_state = value_to_pentarny(score)
        return self.set_state(p_state.value, confidence=confidence,
                              metadata={'source': 'ternary_clarify',
                                        'candidate': best_candidate,
                                        'raw_score': score})

    def set_l5_state(self, layer: str, value: int, confidence: float = 0.5,
                     metadata: Optional[Dict[str, Any]] = None):
        layer_map = {l.value: l for l in L5Layer}
        if layer not in layer_map:
            raise ValueError(f"Допустимые слои: {', '.join(layer_map)}")
        self.l5.set_state(layer_map[layer], value, confidence, metadata)

    def aggregate_l5_to_state(self, method: str = 'fold_and',
                              confidence: Optional[float] = None) -> Optional[PentarnyStateData]:
        """Свести все выставленные L5-слои в единое current_state.

        method:
          'fold_and' — AND-свёртка (консервативно, тянется к слабому звену).
          'fold_or'  — OR-свёртка (оптимистично, тянется к сильному).
          'average'  — среднее с округлением (прежнее поведение).
        """
        values = [s.value.value for s in self.l5.states.values() if s is not None]
        if not values:
            return None

        if method == 'fold_and':
            result_value = fold_and_scalar(values)
        elif method == 'fold_or':
            result_value = fold_or_scalar(values)
        elif method == 'average':
            avg = sum(values) / len(values)
            result_value = max(-2, min(2, round(avg)))
        else:
            raise ValueError("method должен быть 'fold_and', 'fold_or' или 'average'")

        conf = confidence if confidence is not None else (
            sum(s.confidence for s in self.l5.states.values() if s is not None) / len(values))
        return self.set_state(result_value, confidence=conf,
                              metadata={'source': f'l5_aggregate_{method}',
                                        'input_values': values})

    def evaluate_l5(self) -> Dict:
        return self.l5.evaluate()

    def analyze_rashomon(self) -> Dict:
        if self.current_state is None:
            return {'error': 'Состояние не установлено'}
        return self.rashomon.to_dict(self.current_state)

    def run(self) -> Dict:
        if self.current_state is None:
            return {'status': 'error', 'message': 'Состояние не установлено'}
        return {
            'status': 'ok',
            'current_state': self.current_state.to_dict(),
            'l5': self.l5.evaluate(),
            'rashomon': self.rashomon.to_dict(self.current_state),
            'audit_chain': self.chain.to_dict(),
        }


# ============================================================================
# ТРОИЧНЫЕ МАТРИЦЫ
# ============================================================================

class TernaryMatrix:
    """Матрица значений {-1, 0, 1}. Алгебра: клиппинг в [-1, 1] (L3-алгебра,
    НЕ линейная алгебра — операция multiply насыщается)."""

    def __init__(self, data: List[List[int]]):
        self.data = data
        self.rows = len(data)
        self.cols = len(data[0]) if data else 0
        self._validate()

    def _validate(self):
        if not self.data:
            raise ValueError("Матрица не может быть пустой")
        for row in self.data:
            if len(row) != self.cols:
                raise ValueError(f"Все строки должны быть одинаковой длины")
            for val in row:
                if val not in (-1, 0, 1):
                    raise ValueError(f"Недопустимое значение {val}. Допустимы -1, 0, 1")

    def __repr__(self):
        return f"TernaryMatrix({self.rows}x{self.cols})"

    def __eq__(self, other):
        return isinstance(other, TernaryMatrix) and self.data == other.data

    def copy(self) -> 'TernaryMatrix':
        return TernaryMatrix([row[:] for row in self.data])

    @cached_property
    def hash(self) -> str:
        return hashlib.sha256(json.dumps(self.data).encode()).hexdigest()

    def to_dict(self) -> Dict:
        return {'rows': self.rows, 'cols': self.cols,
                'data': self.data, 'hash': self.hash[:16] + '...'}

    @staticmethod
    def _clip(v: int) -> int:
        return 1 if v > 1 else (-1 if v < -1 else v)

    def add(self, other: 'TernaryMatrix') -> 'TernaryMatrix':
        if (self.rows, self.cols) != (other.rows, other.cols):
            raise ValueError(f"Размеры не совпадают")
        return TernaryMatrix([[self._clip(self.data[i][j] + other.data[i][j])
                               for j in range(self.cols)] for i in range(self.rows)])

    def multiply(self, other: 'TernaryMatrix') -> 'TernaryMatrix':
        """Стандартное матричное умножение, результат клиппирован в L3.
        ВНИМАНИЕ: клиппинг означает потерю информации для сумм > 1."""
        if self.cols != other.rows:
            raise ValueError(f"Несовместимые размеры: {self.cols} != {other.rows}")
        result = []
        for i in range(self.rows):
            row = []
            for j in range(other.cols):
                s = sum(self.data[i][k] * other.data[k][j] for k in range(self.cols))
                row.append(self._clip(s))
            result.append(row)
        return TernaryMatrix(result)

    def transpose(self) -> 'TernaryMatrix':
        return TernaryMatrix([[self.data[j][i] for j in range(self.rows)]
                              for i in range(self.cols)])

    def determinant(self) -> int:
        """Только для 1x1..3x3. Знак нормализован в {-1, 0, 1}."""
        if self.rows != self.cols:
            raise ValueError("Детерминант только для квадратных матриц")
        if self.rows == 1:
            return self.data[0][0]
        if self.rows == 2:
            det = self.data[0][0] * self.data[1][1] - self.data[0][1] * self.data[1][0]
        elif self.rows == 3:
            a, b, c = self.data[0]
            d, e, f = self.data[1]
            g, h, i = self.data[2]
            det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
        else:
            raise ValueError(f"Детерминант для {self.rows}x{self.rows} не реализован")
        return 1 if det > 0 else (-1 if det < 0 else 0)

    def stability_index(self) -> float:
        """(det+1)/2: 1.0 стабильна, 0.5 нейтральна, 0.0 конфликтна."""
        try:
            return (self.determinant() + 1) / 2.0
        except ValueError:
            return 0.5

    def entropy(self) -> float:
        """Энтропия Шеннона по частотам {-1, 0, 1}."""
        ones = sum(row.count(1) for row in self.data)
        zeros = sum(row.count(0) for row in self.data)
        negs = sum(row.count(-1) for row in self.data)
        total = self.rows * self.cols
        if total == 0:
            return 0.0
        ent = 0.0
        for p in (ones / total, zeros / total, negs / total):
            if p > 0:
                ent -= p * math.log2(p)
        return ent

    def diagonal_projection(self) -> 'TernaryMatrix':
        """Обнулить всё, кроме диагонали."""
        size = min(self.rows, self.cols)
        result = [[0] * self.cols for _ in range(self.rows)]
        for i in range(size):
            result[i][i] = self.data[i][i]
        return TernaryMatrix(result)

    def energy_presence(self) -> 'TernaryMatrix':
        """1 где значение ненулевое, 0 где ноль. Имя оставлено для
        совместимости, но это НЕ про энергию — это карта активности."""
        return TernaryMatrix([[1 if v != 0 else 0 for v in row] for row in self.data])


def pentary_to_ternary(value: int) -> int:
    """Пентарное (-2..2) -> троичное (-1, 0, 1)."""
    if value <= -1:
        return -1
    if value >= 1:
        return 1
    return 0


def from_pentary(l5_values: Dict[str, int]) -> TernaryMatrix:
    """Пять пентарных значений L5-слоёв -> диагональная троичная матрица 5x5.
    Порядок слоёв берётся из L5Layer (без дублирования списка)."""
    order = [l.value for l in L5Layer]
    n = len(order)
    data = [[0] * n for _ in range(n)]
    for i, layer in enumerate(order):
        v = l5_values.get(layer, 0)
        data[i][i] = pentary_to_ternary(v)
    return TernaryMatrix(data)


@dataclass
class MatrixChainRecord:
    index: int
    timestamp: float
    prev_hash: str
    matrix: TernaryMatrix
    payload: Dict[str, Any]

    @cached_property
    def hash(self) -> str:
        data = (f"{self.index}{self.timestamp}{self.prev_hash}"
                f"{self.matrix.hash}{json.dumps(self.payload, sort_keys=True)}")
        return hashlib.sha256(data.encode()).hexdigest()

    def to_dict(self) -> Dict:
        return {'index': self.index, 'timestamp': self.timestamp,
                'prev_hash': self.prev_hash[:16] + '...',
                'matrix': self.matrix.to_dict(),
                'payload': self.payload, 'hash': self.hash[:16] + '...'}


class MatrixAuditChain:
    def __init__(self, max_length: int = 500):
        self.records: List[MatrixChainRecord] = []
        self.max_length = max_length

    def add(self, matrix: TernaryMatrix, payload: Dict[str, Any]) -> MatrixChainRecord:
        record = MatrixChainRecord(
            index=len(self.records), timestamp=time.time(),
            prev_hash=self.records[-1].hash if self.records else 'GENESIS',
            matrix=matrix, payload=payload,
        )
        self.records.append(record)
        if len(self.records) > self.max_length:
            self.records.pop(0)
        return record

    def verify(self) -> Tuple[bool, int]:
        for i, r in enumerate(self.records):
            if i > 0 and r.prev_hash != self.records[i - 1].hash:
                return False, i
        return True, -1

    def to_dict(self) -> Dict:
        verified, err = self.verify()
        return {'length': len(self.records), 'verified': verified,
                'error_index': err if not verified else None}


class TernaryMatrixBlock:
    """Держит текущую матрицу состояния, применяет операции, ведёт
    ограниченную по размеру историю (без майнинга)."""

    _OPS = {
        'transpose': lambda m, other=None: m.transpose(),
        'diagonal_projection': lambda m, other=None: m.diagonal_projection(),
        'energy_presence': lambda m, other=None: m.energy_presence(),
        'add': lambda m, other: m.add(other),
        'multiply': lambda m, other: m.multiply(other),
    }
    _REQUIRES_OTHER = {'add', 'multiply'}

    def __init__(self, initial: Optional[TernaryMatrix] = None):
        self.state = initial or TernaryMatrix([[0, 0, 0], [0, 1, 0], [0, 0, 0]])
        self.chain = MatrixAuditChain()

    def apply(self, operation: str, other: Optional[TernaryMatrix] = None) -> TernaryMatrix:
        if operation not in self._OPS:
            raise ValueError(f"Неизвестная операция: {operation}")
        if operation in self._REQUIRES_OTHER and other is None:
            raise ValueError(f"Операция '{operation}' требует вторую матрицу")

        old_stability = self.state.stability_index()
        self.state = self._OPS[operation](self.state, other)
        self.chain.add(self.state, {
            'operation': operation,
            'old_stability': old_stability,
            'new_stability': self.state.stability_index(),
        })
        return self.state

    def analyze(self) -> Dict[str, Any]:
        return {
            'matrix': self.state.to_dict(),
            'stability': round(self.state.stability_index(), 3),
            'entropy': round(self.state.entropy(), 3),
            'determinant': (self.state.determinant()
                            if self.state.rows == self.state.cols and self.state.rows <= 3
                            else None),
            'audit_chain': self.chain.to_dict(),
        }


# ============================================================================
# BXOS-ГЕЙТ (единственный в этом файле)
# ============================================================================

class GateStatus(Enum):
    READY = "ready_for_simulation"
    LOCKED = "locked_critical"


@dataclass
class GateResult:
    status: GateStatus
    stage: str  # "input" | "output"
    reason: str
    timestamp: float


class BXOSGate:
    """Два детерминированных чек-пойнта:
      check_input()  — сырые интерпретации ДО обработки пайплайном
      check_output() — итоговое пентарное состояние ПОСЛЕ обработки

    Никакого I/O, никакого sleep, никакой геометрии — только сравнения
    чисел. Лог ограничен по размеру.
    """

    def __init__(self, margin_threshold: float = 0.15, min_score: float = 0.3,
                 min_confidence: float = 0.4, critical_pentary_states=(-2, -1),
                 max_log: int = 1000):
        self.margin_threshold = margin_threshold
        self.min_score = min_score
        self.min_confidence = min_confidence
        self.critical_pentary_states = set(critical_pentary_states)
        self.max_log = max_log
        self.log: List[GateResult] = []

    def _record(self, result: GateResult) -> GateResult:
        self.log.append(result)
        if len(self.log) > self.max_log:
            self.log.pop(0)
        return result

    def check_input(self, interpretations: Dict[str, float]) -> GateResult:
        """Входной чек-пойнт: явно негодные интерпретации блокируются
        до того, как пайплайн потратит на них хоть один цикл."""
        analysis = rashomon_analyze(interpretations, self.margin_threshold, self.min_score)
        status = GateStatus.LOCKED if analysis['state'] == -1 else GateStatus.READY
        return self._record(GateResult(status=status, stage="input",
                                       reason=analysis['reason'], timestamp=time.time()))

    def check_output(self, pentary_value: int, confidence: float) -> GateResult:
        """Выходной чек-пойнт: итоговое состояние и уверенность в нём."""
        if pentary_value not in (-2, -1, 0, 1, 2):
            raise ValueError("pentary_value должен быть в диапазоне -2..2")

        if pentary_value in self.critical_pentary_states:
            reason = f"критическое состояние на выходе: {pentary_value}"
            status = GateStatus.LOCKED
        elif confidence < self.min_confidence:
            reason = f"уверенность ниже порога: {confidence:.2f} < {self.min_confidence}"
            status = GateStatus.LOCKED
        else:
            reason = "выход прошёл проверку"
            status = GateStatus.READY

        return self._record(GateResult(status=status, stage="output",
                                       reason=reason, timestamp=time.time()))

    def summary(self) -> Dict[str, Any]:
        locked = sum(1 for r in self.log if r.status == GateStatus.LOCKED)
        ready = len(self.log) - locked
        return {'total': len(self.log), 'ready': ready, 'locked': locked}


# ============================================================================
# ПОЛНЫЙ ПАЙПЛАЙН — ЛУКАСЕВИЧ
# ============================================================================

def trit_to_confidence(trit: int) -> float:
    return {-1: 0.3, 0: 0.55, 1: 0.85}[trit]


@dataclass
class FullPipelineResult:
    status: str
    ternary: Optional[TernaryResult]
    consolidated_trit: Optional[int]
    pentary_confidence: Optional[float]
    pentary_state: Optional[PentarnyStateData]
    matrix: Optional[TernaryMatrix]
    l5_used: bool
    gate_input: Optional[GateResult]
    gate_output: Optional[GateResult]


class FullPipeline:
    """Полный пайплайн на логике Лукасевича (сильная конъюнкция).

    use_trait_table=False по умолчанию: трайт-таблица проигрывает скаляру
    на одиночных вызовах (её выигрыш — только при батч-обработке
    миллионов значений разом, не при 1 запрос = 1 свёртка).
    """

    def __init__(self, use_trait_table: bool = False):
        self.ternary = TernaryBlock()
        self.pentary = PentaryBlock()
        self.matrix = TernaryMatrixBlock()
        self.gate = BXOSGate()
        self.use_trait_table = use_trait_table

    def process(self, query: str, interpretations: Dict[str, float],
                l5_values: Optional[Dict[str, int]] = None) -> FullPipelineResult:
        # 0. BXOS: входной гейт
        gate_in = self.gate.check_input(interpretations)
        if gate_in.status == GateStatus.LOCKED:
            return FullPipelineResult(status="locked_input", ternary=None,
                                      consolidated_trit=None,
                                      pentary_confidence=None, pentary_state=None,
                                      matrix=None, l5_used=False,
                                      gate_input=gate_in, gate_output=None)

        # 1. ТРОИЧНЫЙ БЛОК
        tr = self.ternary.process(query, interpretations)
        if tr.final_decision == "REJECT":
            return FullPipelineResult(status="rejected", ternary=tr,
                                      consolidated_trit=None,
                                      pentary_confidence=None, pentary_state=None,
                                      matrix=None, l5_used=False,
                                      gate_input=gate_in, gate_output=None)

        # 2. ЛУКАСЕВИЧ: свёртка трёх тритов
        trits = (tr.l3_in, tr.l3_out, tr.efficiency_trit)
        consolidated = fold_and(trits)
        confidence = trit_to_confidence(consolidated)

        # 3. ПЕНТАРНЫЙ БЛОК
        pentary_state = self.pentary.receive_from_ternary(
            best_candidate=tr.best_candidate, score=tr.efficiency,
            confidence=confidence,
        )

        # 4. (ОПЦИОНАЛЬНО) ТРОИЧНЫЕ МАТРИЦЫ
        matrix_result = None
        l5_used = False
        if l5_values and set(l5_values.keys()) == {l.value for l in L5Layer}:
            for layer, value in l5_values.items():
                self.pentary.set_l5_state(layer, value)
            l5_used = True
            mat = from_pentary(l5_values)
            self.matrix.state = mat
            self.matrix.apply('diagonal_projection')
            matrix_result = self.matrix.state

        # 5. BXOS: выходной гейт
        gate_out = self.gate.check_output(pentary_value=pentary_state.value.value,
                                          confidence=confidence)

        return FullPipelineResult(
            status="locked_output" if gate_out.status == GateStatus.LOCKED else "ok",
            ternary=tr, consolidated_trit=consolidated,
            pentary_confidence=confidence,
            pentary_state=pentary_state, matrix=matrix_result, l5_used=l5_used,
            gate_input=gate_in, gate_output=gate_out,
        )


# ============================================================================
# ПОЛНЫЙ ПАЙПЛАЙН — ГЁДЕЛЬ/КЛИНИ
# ============================================================================

@dataclass
class GodelPipelineResult:
    status: str
    ternary: Optional[TernaryResult]
    consolidated_trit: Optional[int]
    pentary_confidence: Optional[float]
    pentary_state: Optional[PentarnyStateData]
    matrix: Optional[TernaryMatrix]
    l5_used: bool
    gate_input: Optional[GateResult]
    gate_output: Optional[GateResult]


class GodelPipeline:
    """Полный пайплайн на логике Гёделя/Клини (min/max).
    API идентичен FullPipeline — можно менять местами."""

    def __init__(self):
        self.ternary = TernaryBlock()
        self.pentary = PentaryBlock()
        self.matrix = TernaryMatrixBlock()
        self.gate = BXOSGate()

    def process(self, query: str, interpretations: Dict[str, float],
                l5_values: Optional[Dict[str, int]] = None) -> GodelPipelineResult:
        gate_in = self.gate.check_input(interpretations)
        if gate_in.status == GateStatus.LOCKED:
            return GodelPipelineResult(status="locked_input", ternary=None,
                                       consolidated_trit=None,
                                       pentary_confidence=None, pentary_state=None,
                                       matrix=None, l5_used=False,
                                       gate_input=gate_in, gate_output=None)

        tr = self.ternary.process(query, interpretations)
        if tr.final_decision == "REJECT":
            return GodelPipelineResult(status="rejected", ternary=tr,
                                       consolidated_trit=None,
                                       pentary_confidence=None, pentary_state=None,
                                       matrix=None, l5_used=False,
                                       gate_input=gate_in, gate_output=None)

        # ГЁДЕЛЬ/КЛИНИ: свёртка через min
        trits = [tr.l3_in, tr.l3_out, tr.efficiency_trit]
        consolidated = godel_fold_and_scalar(trits)
        confidence = trit_to_confidence(consolidated)

        pentary_state = self.pentary.receive_from_ternary(
            best_candidate=tr.best_candidate, score=tr.efficiency,
            confidence=confidence,
        )

        matrix_result = None
        l5_used = False
        if l5_values and set(l5_values.keys()) == {l.value for l in L5Layer}:
            for layer, value in l5_values.items():
                self.pentary.set_l5_state(layer, value)
            l5_used = True
            mat = from_pentary(l5_values)
            self.matrix.state = mat
            self.matrix.apply('diagonal_projection')
            matrix_result = self.matrix.state

        gate_out = self.gate.check_output(pentary_value=pentary_state.value.value,
                                          confidence=confidence)

        return GodelPipelineResult(
            status="locked_output" if gate_out.status == GateStatus.LOCKED else "ok",
            ternary=tr, consolidated_trit=consolidated,
            pentary_confidence=confidence,
            pentary_state=pentary_state, matrix=matrix_result, l5_used=l5_used,
            gate_input=gate_in, gate_output=gate_out,
        )

    def status(self) -> Dict[str, Any]:
        """Оркестраторская сводка — агрегирует аудит-цепочки всех блоков."""
        return {
            'logic': 'Гёдель/Клини (min/max)',
            'pentary_audit_chain': self.pentary.chain.to_dict(),
            'matrix_audit_chain': self.matrix.chain.to_dict(),
            'bxos_summary': self.gate.summary(),
        }


def batch_consolidate(l3_in: np.ndarray, l3_out: np.ndarray, eff_trit: np.ndarray) -> np.ndarray:
    """Векторизованная версия ТОЧНО ТОЙ ЖЕ свёртки, что в GodelPipeline.process
    — используется ТОЛЬКО для проверки scalar==batch."""
    stacked = np.stack([l3_in, l3_out, eff_trit], axis=1).astype(np.int8)
    return godel_fold_and_batch(stacked)


# ============================================================================
# ОБЯЗАТЕЛЬНЫЕ САМОПРОВЕРКИ
# ============================================================================

def self_check_scalar_batch(n_trials: int = 20_000, seed: int = 0) -> int:
    """Проверка: scalar-путь и батч-версия ТОЙ ЖЕ формулы совпадают."""
    rng = np.random.default_rng(seed)
    l3_in = rng.integers(-1, 2, size=n_trials).astype(np.int8)
    l3_out = rng.integers(-1, 2, size=n_trials).astype(np.int8)
    eff_trit = rng.integers(-1, 2, size=n_trials).astype(np.int8)

    batch_result = batch_consolidate(l3_in, l3_out, eff_trit)

    mismatches = 0
    for i in range(n_trials):
        scalar_result = godel_fold_and_scalar(
            [int(l3_in[i]), int(l3_out[i]), int(eff_trit[i])]
        )
        if scalar_result != batch_result[i]:
            mismatches += 1
    return mismatches


def self_check_lukasiewicz_associativity() -> int:
    """Проверка ассоциативности сильной конъюнкции Лукасевича на всех
    27 тройках {-1, 0, 1}.

    ВАЖНО: сильная конъюнкция Лукасевича НЕ гарантированно ассоциативна
    на всех входных данных. Эта проверка показывает, есть ли нарушения.
    Если нарушений > 0 — fold_and для Лукасевича зависит от порядка
    применения и должна вызываться с фиксированным порядком.
    """
    violations = 0
    for a, b, c in iproduct(range(-1, 2), repeat=3):
        left = trit_and(trit_and(a, b), c)
        right = trit_and(a, trit_and(b, c))
        if left != right:
            violations += 1
    return violations


def self_check_godel_associativity() -> int:
    """Контрольная проверка: Гёдель/Клини (min) ассоциативен
    математически. Ожидается 0 нарушений."""
    fails = 0
    for a, b, c in iproduct(range(-2, 3), repeat=3):
        if godel_and(godel_and(a, b), c) != godel_and(a, godel_and(b, c)):
            fails += 1
    return fails


def self_check_godel_demorgan() -> int:
    fails = 0
    for a, b in iproduct(range(-2, 3), repeat=2):
        if godel_not(godel_and(a, b)) != godel_or(godel_not(a), godel_not(b)):
            fails += 1
    return fails


def run_self_checks() -> bool:
    print("=" * 78)
    print("ШАГ 1: ОБЯЗАТЕЛЬНЫЕ САМОПРОВЕРКИ")
    print("=" * 78)

    f_godel = self_check_godel_associativity()
    print(f"Гёдель:   ассоциативность AND на 125 тройках: нарушений = {f_godel}")

    f_dm = self_check_godel_demorgan()
    print(f"Гёдель:   Де Морган на 25 парах: нарушений = {f_dm}")

    f_luka = self_check_lukasiewicz_associativity()
    print(f"Лукасевич: ассоциативность AND на 27 тройках: нарушений = {f_luka}")
    if f_luka > 0:
        print(f"           -> ВНИМАНИЕ: fold_and Лукасевича зависит от порядка!")

    mismatches = self_check_scalar_batch(n_trials=20_000)
    print(f"Гёдель:   scalar == batch на 20,000 троек: расхождений = {mismatches}")

    # Эталонная проверка Лукасевича
    ref_trits = (1, 1, 1)
    ref_expected = 1
    luka_check = fold_and(ref_trits)
    luka_ok = (luka_check == ref_expected)
    print(f"Лукасевич: fold_and{ref_trits} = {luka_check} "
          f"(ожидалось {ref_expected}): {'OK' if luka_ok else 'ОШИБКА'}")

    ok = (f_godel == 0 and f_dm == 0 and mismatches == 0 and luka_ok)
    print("✅ Все самопроверки пройдены" if ok else "⚠️ ЕСТЬ ПРОБЛЕМЫ")
    return ok


# ============================================================================
# ДЕМОНСТРАЦИЯ И СРАВНЕНИЕ
# ============================================================================

QUERY = "Хочу телефон с хорошей камерой"
INTERPRETATIONS = {"Камера": 0.9, "Цена": 0.4, "Автономность": 0.6, "Бренд": 0.7}
REJECT_CASE = {"A": 0.1, "B": 0.15}


def compare_behavior():
    print("\n" + "=" * 78)
    print("ШАГ 2: СРАВНЕНИЕ ПОВЕДЕНИЯ НА ОДНИХ И ТЕХ ЖЕ ДАННЫХ")
    print("=" * 78)

    godel = GodelPipeline()
    luka = FullPipeline(use_trait_table=False)

    print(f"\n--- Обычный запрос: {QUERY!r} ---")
    rg = godel.process(QUERY, INTERPRETATIONS)
    rl = luka.process(QUERY, INTERPRETATIONS)
    print(f"  Гёдель:     статус={rg.status:10s} свёртка={rg.consolidated_trit:2d} "
          f"пентарно={rg.pentary_state.value.name}")
    print(f"  Лукасевич:  статус={rl.status:10s} свёртка={rl.consolidated_trit:2d} "
          f"пентарно={rl.pentary_state.value.name}")

    print(f"\n--- Явный брак: {REJECT_CASE} (оба должны отклонить) ---")
    rg2 = godel.process("тест", REJECT_CASE)
    rl2 = luka.process("тест", REJECT_CASE)
    print(f"  Гёдель:    статус={rg2.status}")
    print(f"  Лукасевич: статус={rl2.status}")

    print("\n--- Ключевое различие: свёртка тритов ---")
    for trits in [(1, 1, 1), (0, 0, 0), (1, -1, 0)]:
        g = godel_fold_and_scalar(list(trits))
        l = fold_and(trits)
        marker = "  <-- РАЗНЫЕ" if g != l else ""
        print(f"  Триты {trits}: Гёдель={g:2d}  Лукасевич={l:2d}{marker}")

    print("\n--- Оркестраторская сводка (GodelPipeline.status()) ---")
    st = godel.status()
    print(f"  Логика: {st['logic']}")
    print(f"  BXOS: {st['bxos_summary']}")


def benchmark_speed(n_calls: int = 20_000):
    print("\n" + "=" * 78)
    print(f"ШАГ 3: СКОРОСТЬ ({n_calls:,} вызовов каждый)")
    print("=" * 78)

    godel = GodelPipeline()
    luka = FullPipeline(use_trait_table=False)

    for _ in range(200):
        godel.process(QUERY, INTERPRETATIONS)
        luka.process(QUERY, INTERPRETATIONS)

    t0 = time.perf_counter()
    for _ in range(n_calls):
        godel.process(QUERY, INTERPRETATIONS)
    dt_godel = time.perf_counter() - t0

    t0 = time.perf_counter()
    for _ in range(n_calls):
        luka.process(QUERY, INTERPRETATIONS)
    dt_luka = time.perf_counter() - t0

    print(f"\n{'Пайплайн':<15} {'мкс/вызов':>12} {'решений/сек':>15}")
    print(f"{'Гёдель':<15} {dt_godel / n_calls * 1e6:>12.2f} {n_calls / dt_godel:>15,.0f}")
    print(f"{'Лукасевич':<15} {dt_luka / n_calls * 1e6:>12.2f} {n_calls / dt_luka:>15,.0f}")
    diff_pct = (dt_luka / dt_godel - 1) * 100
    label = 'Гёдель быстрее' if diff_pct > 0 else 'Лукасевич быстрее'
    print(f"\nРазница: {diff_pct:+.1f}% ({label})")


def demo_json_output():
    """Демонстрация: вывод результата в JSON."""
    godel = GodelPipeline()
    r = godel.process(QUERY, INTERPRETATIONS)
    payload = {
        'status': r.status,
        'consolidated_trit': r.consolidated_trit,
        'pentary_confidence': r.pentary_confidence,
        'pentary_state': r.pentary_state.to_dict() if r.pentary_state else None,
        'gate_input_status': r.gate_input.status.value if r.gate_input else None,
        'gate_output_status': r.gate_output.status.value if r.gate_output else None,
    }
    print(json.dumps(payload, ensure_ascii=False, indent=2, default=str))


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        prog='godel_lukasiewicz_pipeline.py',
        description='Trinary-pentary pipeline (Gödel/Kleene + Łukasiewicz). '
                    'Author: Vladimir Zavodiuk. All rights reserved.',
    )
    parser.add_argument('--readme', action='store_true',
                        help='показать только README (docstring)')
    parser.add_argument('--benchmark', action='store_true',
                        help='добавить бенчмарк скорости')
    parser.add_argument('--json', action='store_true',
                        help='вывести результат одного запроса в JSON')
    parser.add_argument('--skip-checks', action='store_true',
                        help='пропустить самопроверки (НЕ РЕКОМЕНДУЕТСЯ)')
    args = parser.parse_args()

    if args.readme:
        print(__doc__)
        return 0

    print("=" * 78)
    print("  TRINARY-PENTARY PIPELINE (Gödel/Kleene + Łukasiewicz)")
    print("  Author: Vladimir Zavodiuk. All rights reserved.")
    print("=" * 78)

    if not args.skip_checks:
        ok = run_self_checks()
        if not ok:
            print("\nОстановлено: самопроверка провалена.")
            return 1

    compare_behavior()

    if args.json:
        print("\n" + "=" * 78)
        print("  JSON-ВЫВОД (один запрос)")
        print("=" * 78)
        demo_json_output()

    if args.benchmark:
        benchmark_speed()

    print("\n✅ Демонстрация завершена")
    return 0


if __name__ == "__main__":
    sys.exit(main())
