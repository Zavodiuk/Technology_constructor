# -*- coding: utf-8 -*-
"""ZVD-TETRA («14.10 ZVD Ultimate data center»): проверка «без потерь» байт в байт.

Пока файл не разбирается Python (с 311-й строки потеряны отступы и двойные
подчёркивания при копировании через чат), тесты пропускаются с точной причиной.
Как только файл загрузится — тесты сразу заработают.
"""
import numpy as np
import pytest

from conftest import load_author_module, quiet
from tools.roundtrip import distinct_inputs_distinct_codes

ZVD = "14.10 ZVD Ultimate data center"


@pytest.fixture(scope="module")
def zvd():
    pytest.importorskip("numba", reason="для ZVD нужен numba: pip install numba")
    return load_author_module(ZVD, "zvd_author")


def test_bitstream_roundtrip(zvd, tmp_path):
    """Контейнер .zvd5t: кубы, записанные и прочитанные, совпадают байт в байт."""
    rng = np.random.default_rng(0)
    cubes = []
    for _ in range(3):
        d = rng.integers(-2, 3, (5, 5, 5, 4)).astype(np.int8)
        s = rng.integers(-2, 3, (5, 5, 5, 4)).astype(np.int8)
        cubes.append(zvd.TetraCube5(data=d, semantic_data=s))
    path = tmp_path / "t.zvd5t"
    w = zvd.TetraBitstreamWriter(str(path), lossless_mode=True)
    w.write_gop(cubes, 1, 3)
    w.close()
    r = zvd.TetraBitstreamReader(str(path))
    back, *_ = r.read_gop()
    r.close()
    for a, b in zip(cubes, back):
        assert a.pack_monolith(lossless=True) == b.pack_monolith(lossless=True)


@pytest.mark.xfail(reason="известное поведение: остаток считается от копии оригинала → всегда нули", strict=False)
def test_text_distinct_inputs_give_distinct_cubes(zvd):
    """Необходимое условие «без потерь»: разные тексты → разные кубы."""
    codec = zvd.LosslessTextVolumeCodec(zvd.W4ProductionPipeline(strategy="luka"))
    with quiet():
        assert distinct_inputs_distinct_codes(
            lambda raw: codec.pack_lossless_text(raw.decode("utf-8")),
            "Перевести 1000 евро Ивану".encode(), "Перевести 9000 евро Петру".encode())


@pytest.mark.xfail(reason="в коде нет декодера текста из куба", strict=False)
def test_text_roundtrip(zvd):
    assert hasattr(zvd.LosslessTextVolumeCodec, "unpack_lossless_text")


@pytest.mark.xfail(reason="предсказание (среднее блока) не сохраняется → кадр не восстановить; нет декодера", strict=False)
def test_video_roundtrip(zvd):
    assert hasattr(zvd.LosslessVideoVolumeCodec, "decode_cubes")
