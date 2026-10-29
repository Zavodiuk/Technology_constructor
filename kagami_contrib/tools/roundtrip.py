# -*- coding: utf-8 -*-
"""Универсальная проверка кодека «без потерь»: закодировать → раскодировать → сравнить байт в байт.

    from tools.roundtrip import check
    report = check(encode, decode, samples)

encode(bytes) -> что угодно, decode(то, что вернул encode) -> bytes.
Возвращает отчёт: сколько образцов совпало, где первое расхождение, размеры до и после.
"""
from dataclasses import dataclass, field
from typing import Any, Callable, Iterable, List


@dataclass
class Report:
    total: int = 0
    exact: int = 0
    failures: List[dict] = field(default_factory=list)
    bytes_in: int = 0
    bytes_out: int = 0

    @property
    def lossless(self) -> bool:
        return self.total > 0 and self.exact == self.total

    def __str__(self) -> str:
        ratio = self.bytes_out / self.bytes_in if self.bytes_in else 0
        s = (f"образцов {self.total}, совпало байт в байт {self.exact}; "
             f"размер {self.bytes_in} → {self.bytes_out} байт (×{ratio:.2f})")
        for f in self.failures[:5]:
            s += f"\n  ✗ образец {f['i']}: {f['why']}"
        return s


def _size(x: Any) -> int:
    if isinstance(x, (bytes, bytearray)):
        return len(x)
    if hasattr(x, "nbytes"):
        return int(x.nbytes)
    if isinstance(x, (list, tuple)):
        return sum(_size(i) for i in x)
    for attr in ("pack_monolith",):
        if hasattr(x, attr):
            return len(getattr(x, attr)())
    return 0


def check(encode: Callable[[bytes], Any], decode: Callable[[Any], bytes],
          samples: Iterable[bytes]) -> Report:
    r = Report()
    for i, raw in enumerate(samples):
        r.total += 1
        r.bytes_in += len(raw)
        try:
            enc = encode(raw)
            r.bytes_out += _size(enc)
            back = bytes(decode(enc))
        except Exception as e:  # noqa: BLE001
            r.failures.append({"i": i, "why": f"исключение {type(e).__name__}: {e}"})
            continue
        if back == raw:
            r.exact += 1
        else:
            pos = next((k for k, (a, b) in enumerate(zip(raw, back)) if a != b), min(len(raw), len(back)))
            r.failures.append({"i": i, "why": f"первое расхождение на байте {pos} "
                                              f"(длина {len(raw)} → {len(back)})"})
    return r


def distinct_inputs_distinct_codes(encode: Callable[[bytes], Any], a: bytes, b: bytes) -> bool:
    """Необходимое условие любого кодека без потерь: разные входы → разные коды."""
    ea, eb = encode(a), encode(b)
    pa = ea.pack_monolith(lossless=True) if hasattr(ea, "pack_monolith") else bytes(ea)
    pb = eb.pack_monolith(lossless=True) if hasattr(eb, "pack_monolith") else bytes(eb)
    return pa != pb
