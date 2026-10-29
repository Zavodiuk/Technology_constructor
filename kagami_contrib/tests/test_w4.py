# -*- coding: utf-8 -*-
"""W4 («14.09 W4»): собственные самопроверки автора + поведение обоих вариантов логики."""
import itertools
import random

import pytest

from conftest import load_author_module, quiet

W4 = "14.09 W4"


@pytest.fixture(scope="module")
def w4():
    return load_author_module(W4, "w4_author")


def test_author_self_checks_pass(w4):
    """Самопроверки автора: ассоциативность, Де Морган, scalar == batch."""
    with quiet():
        assert w4.run_self_checks() is True


def test_godel_and_is_min(w4):
    for a, b in itertools.product((-1, 0, 1), repeat=2):
        assert w4.godel_fold_and_scalar([a, b]) == min(a, b)


def test_lukasiewicz_key_difference(w4):
    """Ключевое различие, описанное автором: (0,0,0) → Гёдель 0, Лукасевич −1."""
    assert w4.godel_fold_and_scalar([0, 0, 0]) == 0
    assert w4.fold_and((0, 0, 0)) == -1
    assert w4.fold_and((1, 1, 1)) == 1


CASES = {
    "уверена":     ({"A": 0.85, "B": 0.08, "C": 0.05, "D": 0.02}, "ok", "ok"),
    "средне":      ({"A": 0.60, "B": 0.30, "C": 0.05, "D": 0.05}, "ok", "locked_output"),
    "сомневается": ({"A": 0.45, "B": 0.40, "C": 0.10, "D": 0.05}, "ok", "locked_output"),
    "не знает":    ({"A": 0.28, "B": 0.26, "C": 0.24, "D": 0.22}, "locked_input", "locked_input"),
}


@pytest.mark.parametrize("case", list(CASES))
def test_answer_probabilities_as_input(w4, case):
    """Вероятности вариантов A–D от языковой модели подаются в W4 напрямую."""
    probs, want_g, want_l = CASES[case]
    g = w4.GodelPipeline().process("q", probs)
    l = w4.FullPipeline().process("q", probs)
    assert g.status == want_g
    assert l.status == want_l


def test_best_candidate_is_most_probable(w4):
    """Если W4 отвечает, он выбирает самый вероятный вариант."""
    rnd = random.Random(0)
    for _ in range(500):
        p = [rnd.random() for _ in range(4)]
        s = sum(p)
        probs = {k: v / s for k, v in zip("ABCD", p)}
        r = w4.GodelPipeline().process("q", probs)
        if r.status == "ok":
            assert r.ternary.best_candidate == max(probs, key=probs.get)


def test_deterministic(w4):
    probs = CASES["средне"][0]
    a = [w4.FullPipeline().process("q", probs).status for _ in range(5)]
    assert len(set(a)) == 1
