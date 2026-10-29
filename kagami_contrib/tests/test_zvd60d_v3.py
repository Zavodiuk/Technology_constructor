# -*- coding: utf-8 -*-
"""Тесты к «28.10 ZVD_ACCEL_TETRA_ULTIMATE 60D v3» — актуальная версия по словам автора.

Файл автора загружается как есть, без изменений.
passed  — работает как ожидается;
xfail   — известная особенность, причина записана в тесте (когда исправят — станет XPASS).
"""
import numpy as np
import pytest

from conftest import load_author_module, quiet
from tools.roundtrip import check, distinct_inputs_distinct_codes

V3 = "28.10 ZVD_ACCEL_TETRA_ULTIMATE 60D v3"


@pytest.fixture(scope="module")
def v3():
    pytest.importorskip("numba", reason="нужен numba: pip install numba")
    pytest.importorskip("zstd", reason="нужен zstd: pip install zstd")
    return load_author_module(V3, "zvd60d_v3_author")


# ---------------------------------------------------------------- 60D-логика

def test_dim_60_is_lcm_of_3_4_5(v3):
    """60 — наименьшее общее кратное 3, 4 и 5 (основа 60D)."""
    import math
    assert v3.DIM_60 == 60 == math.lcm(3, 4, 5)


def test_godel_fold_is_minimum(v3):
    rng = np.random.default_rng(1)
    eng = v3.MultivaluedLogicEngine60D("godel")
    for _ in range(50):
        vec = rng.integers(-2, 3, 60).astype(np.int8)
        assert eng.fold(vec.copy()) == int(vec.min())


def test_luka_fold_formula(v3):
    """Лукасевич: max(-2, Σx − 2·(n−1)). Для n = 60 это Σx − 118."""
    eng = v3.MultivaluedLogicEngine60D("luka")
    assert eng.fold(np.full(60, 2, np.int8)) == 2
    assert eng.fold(np.full(60, 1, np.int8)) == -2


def test_luka_60d_collapses_to_minus_two(v3):
    """Свойство (не ошибка кода): при 60 значениях Лукасевич даёт −2,
    если хотя бы четыре значения равны +1 вместо +2 (Σ = 116 → 116 − 118 = −2). Под нагрузкой (stress ≥ 0.70)
    конвейер переходит на Лукасевича — и почти всё сворачивается в −2."""
    eng = v3.MultivaluedLogicEngine60D("luka")
    vec = np.full(60, 2, np.int8)
    vec[:3] = 1
    assert eng.fold(vec) == -1
    vec[:4] = 1
    assert eng.fold(vec) == -2


def test_ontology_60d_carries_only_5_values(v3):
    """L5OntologyLayer60D строит 60 значений из 5 входов (12 повторов, половина со сдвигом +1).
    Тест фиксирует это: разные 60D-векторы определяются только 5 числами."""
    onto = v3.L5OntologyLayer60D()
    vec = np.array(onto.update([1, -1, 0, 2, -2]), dtype=int)
    base = np.array([1, -1, 0, 2, -2])
    for i in range(60):
        expected = int(np.clip(base[i % 5] + ((i // 5) - 6) % 2, -2, 2))
        assert vec[i] == expected


def test_pipeline_switches_strategy_on_stress(v3):
    p = v3.W60DAdaptiveProductionPipeline()
    with quiet():
        calm = p.process("R1", 0.8, [2, 2, 2, 2, 2], current_stress=0.3)
        hot = p.process("R2", 0.8, [2, 2, 2, 2, 2], current_stress=0.9)
    assert calm.bxos_receipt["active_strategy"] == "godel"
    assert hot.bxos_receipt["active_strategy"] == "luka"


def test_pipeline_under_stress_folds_to_minus_two(v3):
    """Следствие свойства выше: уверенный вход [1,1,1,1,1] под нагрузкой → −2."""
    p = v3.W60DAdaptiveProductionPipeline()
    with quiet():
        r = p.process("R3", 0.9, [1, 1, 1, 1, 1], current_stress=0.9)
    assert r.folded_result == -2


def test_triage_rejects_low_score(v3):
    p = v3.W60DAdaptiveProductionPipeline()
    with quiet():
        r = p.process("R4", 0.1, [1, 1, 1, 1, 1])
    assert r.status == "REJECTED_BY_TRIAGE"


# ---------------------------------------------------------------- контейнер

def test_cube_is_1000_bytes(v3):
    assert len(v3.TetraCube5().pack_monolith(lossless=True)) == 1000


def test_bitstream_roundtrip(v3, tmp_path):
    """.zvd5t: кубы, записанные и прочитанные обратно, совпадают байт в байт; SHA-256 проверяется."""
    rng = np.random.default_rng(0)
    cubes = [v3.TetraCube5(data=rng.integers(0, 256, (5, 5, 5, 4)).astype(np.uint8),
                           semantic_data=rng.integers(0, 5, (5, 5, 5, 4)).astype(np.uint8))
             for _ in range(3)]
    path = tmp_path / "t.zvd5t"
    w = v3.TetraBitstreamWriter60D(str(path), lossless_mode=True)
    w.write_gop(cubes, 1, 3)
    w.close()
    r = v3.TetraBitstreamReader60D(str(path))
    back, *_ = r.read_gop()
    r.close()
    assert [c.pack_monolith(True) for c in cubes] == [c.pack_monolith(True) for c in back]


def test_bitstream_detects_corruption(v3, tmp_path):
    path = tmp_path / "t.zvd5t"
    w = v3.TetraBitstreamWriter60D(str(path), lossless_mode=True)
    w.write_gop([v3.TetraCube5()], 1, 1)
    w.close()
    raw = bytearray(path.read_bytes())
    raw[-1] ^= 0xFF
    path.write_bytes(bytes(raw))
    r = v3.TetraBitstreamReader60D(str(path))
    with pytest.raises(ValueError):
        r.read_gop()
    r.close()


# ---------------------------------------------------------------- текст

TEXTS = ["ZVD-ACCEL-TETRA 60D", "Перевести 1000 евро Ивану", "Шумеры: 60 = НОК(3, 4, 5)"]


@pytest.fixture
def text_codec(v3):
    return v3.LosslessTextVolumeCodec60D(v3.W60DAdaptiveProductionPipeline())


@pytest.mark.xfail(reason="строки 561–562: reconstructed_bytes = source_bytes → остаток всегда 0, "
                          "текст восстанавливается нулями. Удалить эти две строки.", strict=False)
def test_text_roundtrip(text_codec):
    with quiet():
        rep = check(lambda b: text_codec.pack_lossless_text(b.decode("utf-8")),
                    lambda c: text_codec.unpack_lossless_text(c).encode("utf-8"),
                    [t.encode("utf-8") for t in TEXTS])
    assert rep.lossless, str(rep)


@pytest.mark.xfail(reason="та же причина: все тексты дают одинаковый куб из нулей", strict=False)
def test_text_distinct_inputs_give_distinct_cubes(text_codec):
    with quiet():
        assert distinct_inputs_distinct_codes(
            lambda b: text_codec.pack_lossless_text(b.decode("utf-8")),
            "Перевести 1000 евро Ивану".encode(), "Перевести 9000 евро Петру".encode())


@pytest.mark.xfail(reason="сейчас сжимаются нули (строки 561–562), поэтому любой текст «помещается»", strict=False)
def test_text_too_long_raises(text_codec):
    """В один куб помещается не больше 498 байт сжатого текста — дальше честная ошибка."""
    rng = np.random.default_rng(3)
    big = "".join(chr(c) for c in rng.integers(0x410, 0x44F, 4000))
    with quiet(), pytest.raises(Exception):
        text_codec.pack_lossless_text(big)


@pytest.mark.xfail(reason="зависит от исправления строк 561–562", strict=False)
def test_text_through_bitstream(v3, text_codec, tmp_path):
    with quiet():
        cube = text_codec.pack_lossless_text(TEXTS[1])
    path = tmp_path / "txt.zvd5t"
    w = v3.TetraBitstreamWriter60D(str(path), lossless_mode=True)
    w.write_gop([cube], 1, 1)
    w.close()
    r = v3.TetraBitstreamReader60D(str(path))
    back, *_ = r.read_gop()
    r.close()
    assert text_codec.unpack_lossless_text(back[0]) == TEXTS[1]


# ---------------------------------------------------------------- видео

def _frames(n=5, size=(120, 160)):
    out = []
    for i in range(n):
        img = np.zeros((*size, 3), np.uint8)
        img[25:85, 15 + 4 * i:65 + 4 * i] = (0, 200, 100)
        out.append(img)
    return out


@pytest.fixture
def video(v3):
    pytest.importorskip("cv2", reason="для видео нужен opencv-python")
    return v3.Tetra345VolumeCodec60D((40, 40))


def _encode(video, frames):
    cubes = None
    for f in frames:
        c = video.push_frame(f, curvature=-1.0)
        if c is not None:
            cubes = c
    return cubes


def test_video_pop_frames_returns_5_frames(video):
    cubes = _encode(video, _frames())
    assert cubes is not None and len(cubes) == video.num_cubes
    assert len(video.pop_frames(cubes)) == 5


def test_video_decode_is_deterministic(video, v3):
    cubes = _encode(video, _frames())
    a = video.pop_frames(cubes)
    b = video.pop_frames(cubes)
    assert all(np.array_equal(x, y) for x, y in zip(a, b))


@pytest.mark.xfail(reason="pop_frames возвращает разности между кадрами (5 уровней, −1…+1) "
                          "и только в выбранных точках; сами кадры не восстанавливаются", strict=False)
def test_video_reconstructs_frames(video):
    import cv2
    frames = _frames()
    cubes = _encode(video, frames)
    out = video.pop_frames(cubes)
    for f, d in zip(frames, out):
        g = cv2.cvtColor(cv2.resize(f, (video.w, video.h), interpolation=cv2.INTER_AREA),
                         cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0
        assert float(np.mean(np.abs(g - d))) < 0.05


def test_video_size_report(video, v3):
    """Не проверка, а измерение: сколько байт на сколько точек. Печатается в отчёте (-s)."""
    cubes = _encode(video, _frames())
    pixels_in = 5 * video.h * video.w
    bytes_out = sum(len(c.pack_monolith(True)) for c in cubes)
    print(f"\n[ZVD-60D v3 видео] {pixels_in} точек (5 кадров {video.h}×{video.w}) "
          f"→ {bytes_out} байт в кубах (до zstd)")
    assert bytes_out == 1000 * video.num_cubes
