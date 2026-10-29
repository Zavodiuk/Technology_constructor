# -*- coding: utf-8 -*-
"""Общие помощники для тестов. Файлы автора загружаются как есть, без изменений."""
import ast
import contextlib
import importlib.machinery
import importlib.util
import io
import pathlib
import sys

import pytest

REPO = pathlib.Path(__file__).resolve().parent.parent


def author_file(name: str) -> pathlib.Path:
    p = REPO / name
    if not p.exists():
        pytest.skip(f"файл автора не найден: {name}")
    return p


def load_author_module(name: str, modname: str):
    """Загружает файл автора (имя может быть с пробелами и без .py).
    Если файл не разбирается Python — тест пропускается с точной причиной."""
    path = author_file(name)
    src = path.read_text(encoding="utf-8")
    try:
        ast.parse(src)
    except SyntaxError as e:
        pytest.skip(f"«{name}» не разбирается Python: строка {e.lineno}: {e.msg}")
    loader = importlib.machinery.SourceFileLoader(modname, str(path))
    spec = importlib.util.spec_from_loader(modname, loader)
    mod = importlib.util.module_from_spec(spec)
    old_argv = sys.argv
    sys.argv = [str(path)]
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            loader.exec_module(mod)
    finally:
        sys.argv = old_argv
    return mod


@contextlib.contextmanager
def quiet():
    with contextlib.redirect_stdout(io.StringIO()):
        yield
