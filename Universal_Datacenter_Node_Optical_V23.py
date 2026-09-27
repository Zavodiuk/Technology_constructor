#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# All rights reserved
# Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
# Author: Vladimir Zavodiuk (Root-Architect)
# Version: Universal Datacenter Node + Optical Transport Layer V23.7

"""
================================================================================
UNIVERSAL DATACENTER NODE + OPTICAL TRANSPORT LAYER V23.7
================================================================================

Объединённый узел дата-центра:
  • Вычислительное ядро W4.2 (Гёдель / Лукасевич K=2)
  • Rashomon + 5×5 онтология
  • FastAPI интерфейс
  • Магистральный оптический транспорт (ВОЛС) между узлами

Архитектура:
  [Вычисление батча] → [Опционально: передача по оптике] → [Другой узел]

Автор: Владимир Заводюк
ALL RIGHTS RESERVED
================================================================================
"""

from __future__ import annotations

import sys
import os
import time
import json
import numpy as np
from typing import Dict, Any, List, Optional, Tuple
from collections import deque
from dataclasses import dataclass

# ---------------------------------------------------------------------------
# Зависимости FastAPI
# ---------------------------------------------------------------------------
try:
    from fastapi import FastAPI, HTTPException
    from pydantic import BaseModel, Field
    import uvicorn
except ImportError:
    print("[-] Требуются: fastapi uvicorn pydantic numpy")
    print("    pip install fastapi uvicorn pydantic numpy")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Импорт Оптического Транспортного Слоя (Шаг 1)
# ---------------------------------------------------------------------------
try:
    from Optical_Transport_Layer_V23 import (
        OpticalTransportLayerV23,
        OpticalTransmissionResult
    )
except ImportError:
    # Если файл лежит рядом
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from Optical_Transport_Layer_V23 import (
        OpticalTransportLayerV23,
        OpticalTransmissionResult
    )


# ==============================================================================
# 1. BXOS GATE
# ==============================================================================

class BXOSGate:
    @staticmethod
    def guard(signal: np.ndarray) -> np.ndarray:
        return np.clip(signal, -2.0, 2.0)

    def __init__(self, min_confidence: float = 0.4, critical_states: Tuple[int, ...] = (-2, -1), max_log: int = 1000):
        self.min_confidence = min_confidence
        self.critical_states = set(critical_states)
        self.log: deque = deque(maxlen=max_log)

    def summary(self) -> Dict[str, Any]:
        locked = sum(1 for r in self.log if r.get("status") == "LOCKED")
        ready = len(self.log) - locked
        return {"total": len(self.log), "ready": ready, "locked": locked}


# ==============================================================================
# 2. Логическое ядро W4.2
# ==============================================================================

class MultivaluedLogicEngine:
    def __init__(self, strategy: str = "luka"):
        if strategy not in ("godel", "luka"):
            raise ValueError("strategy must be 'godel' or 'luka'")
        self.strategy = strategy

    def fold(self, data: np.ndarray) -> np.ndarray:
        arr = np.asarray(data, dtype=np.int8)
        clean = BXOSGate.guard(arr.astype(np.float64)).astype(np.int8)

        if clean.ndim == 1:
            if self.strategy == "godel":
                return np.array([int(np.min(clean))])
            n = len(clean)
            total = int(np.sum(clean, dtype=np.int16))
            return np.array([max(-2, total - 2 * (n - 1))])

        if self.strategy == "godel":
            return np.min(clean, axis=-1).astype(np.int8)
        n = clean.shape[-1]
        total = np.sum(clean.astype(np.int16), axis=-1)
        return np.maximum(-2, total - 2 * (n - 1)).astype(np.int8)


class TernaryProcessor:
    @staticmethod
    def analyze_interpretations(
        scores: np.ndarray,
        margin_threshold: float = 0.15,
        min_score: float = 0.3
    ) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        if scores.size == 0:
            return np.array([-1]), np.array([-1]), np.array([0.0])

        # scores: (batch, n_candidates)
        best_idx = np.argmax(scores, axis=1)
        best_scores = scores[np.arange(scores.shape[0]), best_idx]
        sorted_scores = np.sort(scores, axis=1)[:, ::-1]
        margins = sorted_scores[:, 0] - (sorted_scores[:, 1] if scores.shape[1] > 1 else 0.0)

        states = np.zeros(scores.shape[0], dtype=np.int8)
        states[best_scores < min_score] = -1
        states[(best_scores >= min_score) & (margins >= margin_threshold)] = 1
        # 0 остаётся для неопределённости

        return states, best_idx, best_scores


class PentarnyRashomon:
    PERSPECTIVES = ("technical", "operational", "structural", "boundary", "risk")

    @staticmethod
    def evaluate(matrix: np.ndarray) -> Dict[str, Any]:
        if matrix.size == 0:
            return {p: 0.0 for p in PentarnyRashomon.PERSPECTIVES}
        float_m = matrix.astype(float)
        return {
            "technical": float(np.mean(float_m)),
            "operational": float(np.min(float_m)),
            "structural": float(np.max(float_m)),
            "boundary": float(np.std(float_m)),
            "risk": float(-np.mean(np.abs(float_m)))
        }


class PentaryMatrixEngine:
    LAYERS = ["L1_Intent", "L2_Goal", "L3_Ontology", "L4_Context", "L5_Action"]

    def __init__(self):
        self.state = np.zeros((5, 5), dtype=np.int8)

    def update_from_l5(self, l5_list: List[List[int]]) -> np.ndarray:
        arr = np.array(l5_list, dtype=np.int8)
        if arr.ndim == 1:
            arr = arr.reshape(1, -1)
        # Простая проекция в 5x5
        out = np.zeros((arr.shape[0], 5, 5), dtype=np.int8)
        for i, row in enumerate(arr):
            for j, v in enumerate(row[:5]):
                out[i, j, j] = v
        return out


# ==============================================================================
# 3. Вычислительный пайплайн узла
# ==============================================================================

class BXOSPipeline:
    def __init__(self, strategy: str = "luka"):
        self.gate = BXOSGate()
        self.logic = MultivaluedLogicEngine(strategy=strategy)
        self.matrix_engine = PentaryMatrixEngine()
        self.rashomon = PentarnyRashomon()

    def run(self, interpretations_scores: List[List[float]], l5_values_list: List[List[int]]) -> dict:
        scores = np.array(interpretations_scores, dtype=np.float32)
        l5 = np.array(l5_values_list, dtype=np.int8)

        states, best_idx, confidences = TernaryProcessor.analyze_interpretations(scores)

        # Свёртка L5
        folded = self.logic.fold(l5)

        # Матрицы 5x5
        matrices = self.matrix_engine.update_from_l5(l5_values_list)

        # Rashomon по среднему
        audit = self.rashomon.evaluate(l5)

        statuses = []
        for s in states:
            if s == -1:
                statuses.append("LOCKED_INPUT")
            elif s == 0:
                statuses.append("LOCKED_OUTPUT")
            else:
                statuses.append("OK")

        system_energy = float(np.mean(np.abs(l5)) / 2.0)

        return {
            "statuses": statuses,
            "best_candidate_indices": best_idx.tolist(),
            "confidences": confidences.tolist(),
            "folded_results": folded.tolist() if hasattr(folded, "tolist") else list(folded),
            "system_energy": system_energy,
            "rashomon_audit": audit,
            "matrix_shape": [5, 5],
            "ontology_layers": self.matrix_engine.LAYERS,
            "linear_matrices_5x5": matrices.tolist()
        }


# ==============================================================================
# 4. FastAPI + Оптический транспорт
# ==============================================================================

app = FastAPI(
    title="Universal Datacenter Node + Optical Transport V23.7",
    description="Вычислительный узел W4.2 + магистральный оптический канал между нодами",
    version="2.0.0-Optical"
)

# Глобальные объекты
node_pipeline = BXOSPipeline(strategy="luka")
optical_transport = OpticalTransportLayerV23(seed_a=0.33, seed_b=1.95)


class NodePayload(BaseModel):
    interpretations_scores: List[List[float]] = Field(
        ...,
        example=[[0.85, 0.62, 0.41, 0.15], [0.91, 0.44, 0.22, 0.10]]
    )
    l5_values_list: List[List[int]] = Field(
        ...,
        example=[[1, 0, -1, 2, -2], [2, 1, 0, -1, -2]]
    )


class OpticalTransmitRequest(BaseModel):
    payload_b64: Optional[str] = None          # данные в base64 (опционально)
    payload_text: Optional[str] = None         # или обычный текст
    distance_km: float = 100.0
    chromatic_dispersion: float = 0.04
    kerr_nonlinearity: float = 0.015
    osnr_db: float = 26.0


class ComputeAndTransmitRequest(BaseModel):
    """Вычислить батч и сразу отправить результат по оптике"""
    interpretations_scores: List[List[float]]
    l5_values_list: List[List[int]]
    distance_km: float = 150.0
    chromatic_dispersion: float = 0.04
    kerr_nonlinearity: float = 0.015
    osnr_db: float = 26.5


@app.get("/")
def node_status():
    return {
        "author": "Vladimir Zavodiuk",
        "rights": "All rights reserved",
        "project": "Universal Datacenter Node + Optical Transport Layer V23.7",
        "node_id": os.getenv("NODE_ID", "node-01"),
        "node_status": "active",
        "architecture": "W4.2 + 5x5 + Rashomon + Long-Haul Fiber Optic (V23.7)",
        "capabilities": [
            "compute-linear-batch",
            "optical-transmit",
            "compute-and-transmit-optical"
        ],
        "telemetry": node_pipeline.gate.summary()
    }


@app.post("/compute-linear-batch")
def compute_linear_batch(payload: NodePayload):
    """Классический вычислительный эндпоинт"""
    try:
        res = node_pipeline.run(payload.interpretations_scores, payload.l5_values_list)
        return res
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))


@app.post("/optical-transmit")
def optical_transmit(req: OpticalTransmitRequest):
    """
    Передача произвольных данных по моделируемому оптическому каналу.
    """
    import base64

    if req.payload_text:
        payload_bytes = req.payload_text.encode("utf-8")
    elif req.payload_b64:
        payload_bytes = base64.b64decode(req.payload_b64)
    else:
        raise HTTPException(status_code=400, detail="Нужен payload_text или payload_b64")

    result: OpticalTransmissionResult = optical_transport.transmit(
        payload=payload_bytes,
        distance_km=req.distance_km,
        chromatic_dispersion=req.chromatic_dispersion,
        kerr_nonlinearity=req.kerr_nonlinearity,
        osnr_db=req.osnr_db
    )

    return {
        "success": result.success,
        "message": result.message,
        "distance_km": result.distance_km,
        "go_count": result.go_count,
        "revise_count": result.revise_count,
        "no_go_count": result.no_go_count,
        "recovered_text": result.recovered_payload.decode("utf-8", errors="replace"),
        "original_size": len(payload_bytes),
        "recovered_size": len(result.recovered_payload)
    }


@app.post("/compute-and-transmit-optical")
def compute_and_transmit_optical(req: ComputeAndTransmitRequest):
    """
    Главный объединённый сценарий:
    1. Выполнить вычисление батча
    2. Упаковать результат
    3. Передать по оптическому каналу
    4. Вернуть и вычислительный результат, и статус оптической передачи
    """
    # 1. Вычисление
    try:
        compute_result = node_pipeline.run(req.interpretations_scores, req.l5_values_list)
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Compute error: {e}")

    # 2. Упаковка результата в байты
    payload_bytes = json.dumps(compute_result, ensure_ascii=False).encode("utf-8")

    # 3. Передача по оптике
    optical_result: OpticalTransmissionResult = optical_transport.transmit(
        payload=payload_bytes,
        distance_km=req.distance_km,
        chromatic_dispersion=req.chromatic_dispersion,
        kerr_nonlinearity=req.kerr_nonlinearity,
        osnr_db=req.osnr_db
    )

    return {
        "compute": compute_result,
        "optical": {
            "success": optical_result.success,
            "message": optical_result.message,
            "distance_km": optical_result.distance_km,
            "go_count": optical_result.go_count,
            "revise_count": optical_result.revise_count,
            "no_go_count": optical_result.no_go_count,
            "payload_size_bytes": len(payload_bytes)
        }
    }


# ==============================================================================
# Запуск
# ==============================================================================

if __name__ == "__main__":
    print("=" * 78)
    print("Universal Datacenter Node + Optical Transport Layer V23.7")
    print("Author: Vladimir Zavodiuk | All Rights Reserved")
    print("=" * 78)
    print(f"[*] NODE_ID: {os.getenv('NODE_ID', 'node-01')}")
    print("[*] Endpoints:")
    print("    GET  /")
    print("    POST /compute-linear-batch")
    print("    POST /optical-transmit")
    print("    POST /compute-and-transmit-optical")
    print("=" * 78)

    uvicorn.run(app, host="0.0.0.0", port=8000)
