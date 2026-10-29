# -*- coding: utf-8 -*-
"""W5.5+Game («30.09 W5.5+Game»): вход — варианты с вероятностями, выход PASS / CLARIFY / REJECT.

Тесты с пометкой xfail описывают поведение, которое ожидается от контура принятия
решений, но сейчас не выполняется. Когда автор исправит — они станут XPASS.
"""
import random
import time

import pytest

from conftest import load_author_module, quiet

W55 = "30.09 W5.5+Game"


@pytest.fixture(scope="module")
def mod():
    return load_author_module(W55, "w55_author")


def run(contour, probs, q="Вопрос"):
    with quiet():
        return contour.run(q, probs)


def random_probs(rnd):
    p = [rnd.random() for _ in range(4)]
    s = sum(p)
    return {k: v / s for k, v in zip("ABCD", p)}


def test_contract(mod):
    """Есть вход и выход: решение одно из трёх, есть лучший кандидат."""
    r = run(mod.GameW55UnifiedContour(), {"A": 0.85, "B": 0.08, "C": 0.05, "D": 0.02})
    assert r.decision in ("PASS", "CLARIFY", "REJECT")
    assert r.best_candidate in ("A", "B", "C", "D")


def test_very_confident_passes(mod):
    """Очень уверенный ответ (0,97) проходит: PASS."""
    r = run(mod.GameW55UnifiedContour(), {"A": 0.97, "B": 0.01, "C": 0.01, "D": 0.01})
    assert r.decision == "PASS"


@pytest.mark.xfail(reason="известное поведение: уверенный ответ 0,85 даёт CLARIFY (PASS только от ~0,97)", strict=False)
def test_confident_passes(mod):
    r = run(mod.GameW55UnifiedContour(), {"A": 0.85, "B": 0.05, "C": 0.05, "D": 0.05})
    assert r.decision == "PASS"


@pytest.mark.xfail(reason="известное поведение: REJECT не достигается — даже при равных 0,26/0,25/0,25/0,24 решение CLARIFY", strict=False)
def test_lost_is_rejected(mod):
    r = run(mod.GameW55UnifiedContour(), {"A": 0.26, "B": 0.25, "C": 0.25, "D": 0.24})
    assert r.decision == "REJECT"


@pytest.mark.xfail(reason="известное поведение: ~20% случаев лучший кандидат ≠ самый вероятный", strict=False)
def test_best_candidate_is_most_probable(mod):
    rnd = random.Random(0)
    bad = 0
    for _ in range(200):
        probs = random_probs(rnd)
        r = run(mod.GameW55UnifiedContour(), probs)
        bad += r.best_candidate != max(probs, key=probs.get)
    assert bad == 0


@pytest.mark.xfail(reason="известное поведение: накопленная память меняет результат", strict=False)
def test_same_input_same_result_after_history(mod):
    probs = {"A": 0.6, "B": 0.3, "C": 0.05, "D": 0.05}
    fresh = run(mod.GameW55UnifiedContour(), probs)
    c = mod.GameW55UnifiedContour()
    rnd = random.Random(1)
    for _ in range(300):
        run(c, random_probs(rnd))
    after = run(c, probs)
    assert round(after.composite_score, 3) == round(fresh.composite_score, 3)


@pytest.mark.xfail(reason="известное поведение: время вызова растёт с историей", strict=False)
def test_latency_does_not_grow(mod):
    c = mod.GameW55UnifiedContour()
    rnd = random.Random(2)
    def batch(n):
        t = time.perf_counter()
        for _ in range(n):
            run(c, random_probs(rnd))
        return (time.perf_counter() - t) / n
    first = batch(50)
    for _ in range(300):
        run(c, random_probs(rnd))
    last = batch(50)
    assert last < first * 3
