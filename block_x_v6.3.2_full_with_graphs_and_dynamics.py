#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
================================================================================
block_x_v6.3.2_full_with_graphs_and_dynamics.py
================================================================================

ПОЛНАЯ ВЕРСИЯ BLOCK X v6.3.2
- Троичная логика (quantize, ternary_fold)
- Геометрия Лобачевского (гиперболическое пространство)
- Гиперболический граф с динамикой (время, путь, ритм)
- Двойной Rashomon (на входе и выходе)
- Матричный анализ согласия
- Эффективность (value/cost)
- Decoder (Direct / Clarification / Protection)
- Rollback (откат при конфликтах)
- Полный лог состояний
- API-совместимость

АВТОР: Владимир Заводюк
ЛИЦЕНЗИЯ: MIT
ВЕРСИЯ: 6.3.2 (полная)
================================================================================
"""

import math
import time
from typing import List, Tuple, Dict, Optional, Any
from dataclasses import dataclass, field
from enum import Enum

# ============================================================================
# 1. БАЗОВОЕ ЯДРО BLOCK X
# ============================================================================

def quantize(value: float, low: float = 0.3, high: float = 0.7) -> int:
    """Квантует значение [0,1] в трит {-1,0,+1}."""
    if value >= high: return 1
    if value <= low: return -1
    return 0

def normalize_sign(x: int) -> int:
    """Нормализация знака: x>0 → +1, x=0 → 0, x<0 → -1."""
    if x > 0: return 1
    if x < 0: return -1
    return 0

def ternary_fold(vector: Tuple[int, ...]) -> int:
    """Свёртка вектора тритов в один трит."""
    return normalize_sign(sum(vector))

def ternary_diff(vec_a: Tuple[int, ...], vec_b: Tuple[int, ...]) -> Tuple[int, ...]:
    """Поэлементная разность без нормализации (результат [-2..+2])."""
    return tuple(a - b for a, b in zip(vec_a, vec_b))

# ============================================================================
# 2. ГИПЕРБОЛИЧЕСКАЯ ЛОГИКА (ЛОБАЧЕВСКИЙ)
# ============================================================================

def quantize_hyperbolic(value: float, radius: float = 1.0) -> int:
    """Квантование с учётом гиперболического расстояния до центра."""
    if abs(value) >= 1:
        dist = radius * math.atanh(0.99) if value > 0 else -radius * math.atanh(0.99)
    else:
        dist = radius * math.atanh(value)
    return quantize(dist / radius)

def hyperbolic_distance(vec_a: Tuple[float, ...], vec_b: Tuple[float, ...], radius: float = 1.0) -> float:
    """Гиперболическое расстояние в модели Пуанкаре."""
    a = [v / radius for v in vec_a]
    b = [v / radius for v in vec_b]
    
    dot = sum(ai * bi for ai, bi in zip(a, b))
    norm_a = math.sqrt(sum(ai**2 for ai in a))
    norm_b = math.sqrt(sum(bi**2 for bi in b))
    
    if norm_a >= 1 or norm_b >= 1:
        return float('inf')
    
    d = (norm_a - norm_b)**2 / ((1 - norm_a**2) * (1 - norm_b**2))
    d += 2 * dot / ((1 - norm_a**2) * (1 - norm_b**2))
    d = math.sqrt(d)
    
    if d < 1: return 0
    return radius * math.acosh(1 + 2 * d)

def hyperbolic_vector(metrics: List[float], radius: float = 1.0) -> Tuple[float, ...]:
    """Преобразует метрики в гиперболический вектор."""
    trits = [quantize(m) for m in metrics]
    norm = math.sqrt(sum(t**2 for t in trits))
    if norm > radius:
        trits = [t * radius / norm for t in trits]
    return tuple(trits)

# ============================================================================
# 3. ГИПЕРБОЛИЧЕСКИЙ ГРАФ С ДИНАМИКОЙ
# ============================================================================

@dataclass
class HyperbolicNode:
    """Узел гиперболического графа."""
    id: int
    coordinates: Tuple[float, ...]
    label: str
    time: float = 0.0
    value: float = 0.0

@dataclass
class HyperbolicEdge:
    """Ребро гиперболического графа."""
    from_id: int
    to_id: int
    weight: float = 1.0
    dt: float = 1.0

@dataclass
class TrajectoryPoint:
    """Точка траектории с динамикой."""
    node_id: int
    time: float
    path: float
    rhythm: float
    rhythm_trit: int
    path_trit: int

class HyperbolicGraph:
    """
    Гиперболический граф с поддержкой времени, пути и ритма.
    """
    
    def __init__(self, radius: float = 1.0):
        self.radius = radius
        self.nodes: Dict[int, HyperbolicNode] = {}
        self.edges: List[HyperbolicEdge] = []
    
    def add_node(self, node_id: int, coordinates: List[float], label: str = "", time: float = 0.0, value: float = 0.0):
        """Добавляет узел с координатами и меткой."""
        norm = math.sqrt(sum(c**2 for c in coordinates))
        if norm >= self.radius:
            coordinates = [c * self.radius / norm for c in coordinates]
        self.nodes[node_id] = HyperbolicNode(
            id=node_id,
            coordinates=tuple(coordinates),
            label=label or f"Узел {node_id}",
            time=time,
            value=value
        )
    
    def add_edge(self, from_id: int, to_id: int, weight: float = 1.0, dt: float = 1.0):
        """Добавляет ребро с весом и временным шагом."""
        if from_id in self.nodes and to_id in self.nodes:
            self.edges.append(HyperbolicEdge(from_id, to_id, weight, dt))
    
    def get_distance(self, id1: int, id2: int) -> float:
        """Расстояние между двумя узлами."""
        if id1 not in self.nodes or id2 not in self.nodes:
            return float('inf')
        return hyperbolic_distance(self.nodes[id1].coordinates, self.nodes[id2].coordinates, self.radius)
    
    def get_neighbors(self, node_id: int) -> List[Tuple[int, float]]:
        """Возвращает соседей узла с весами рёбер."""
        neighbors = []
        for edge in self.edges:
            if edge.from_id == node_id:
                neighbors.append((edge.to_id, edge.weight))
            elif edge.to_id == node_id:
                neighbors.append((edge.from_id, edge.weight))
        return neighbors
    
    def get_path_metrics(self, id1: int, id2: int) -> Dict:
        """Вычисляет время, путь и ритм между двумя состояниями."""
        if id1 not in self.nodes or id2 not in self.nodes:
            return {'error': 'Узлы не найдены'}
        
        distance = self.get_distance(id1, id2)
        dt = abs(self.nodes[id2].time - self.nodes[id1].time)
        if dt == 0: dt = 0.001
        
        rhythm = distance / dt
        rhythm_trit = quantize(rhythm / self.radius)
        path_trit = quantize(distance / self.radius)
        
        return {
            'path': distance,
            'time': dt,
            'rhythm': rhythm,
            'rhythm_trit': rhythm_trit,
            'path_trit': path_trit
        }
    
    def get_trajectory_rhythm(self, node_ids: List[int]) -> List[Dict]:
        """Анализирует ритм вдоль траектории."""
        results = []
        for i in range(len(node_ids) - 1):
            id1, id2 = node_ids[i], node_ids[i+1]
            metrics = self.get_path_metrics(id1, id2)
            results.append({
                'from': id1,
                'to': id2,
                'path': metrics['path'],
                'time': metrics['time'],
                'rhythm': metrics['rhythm'],
                'rhythm_trit': metrics['rhythm_trit'],
                'path_trit': metrics['path_trit']
            })
        return results
    
    def detect_rhythm_changes(self, node_ids: List[int], threshold: float = 0.5) -> List[Dict]:
        """Находит точки изменения ритма."""
        results = []
        rhythms = []
        
        for i in range(len(node_ids) - 1):
            id1, id2 = node_ids[i], node_ids[i+1]
            metrics = self.get_path_metrics(id1, id2)
            rhythms.append(metrics['rhythm'])
        
        for i in range(1, len(rhythms)):
            change = rhythms[i] - rhythms[i-1]
            if abs(change) > threshold:
                results.append({
                    'index': i,
                    'from_node': node_ids[i],
                    'to_node': node_ids[i+1],
                    'rhythm_change': change,
                    'rhythm_change_trit': quantize(change / self.radius)
                })
        return results
    
    def nearest_nodes(self, vector: Tuple[float, ...], n: int = 3) -> List[Tuple[int, float, str]]:
        """Находит n ближайших узлов к вектору."""
        distances = []
        for node_id, node in self.nodes.items():
            dist = hyperbolic_distance(vector, node.coordinates, self.radius)
            distances.append((node_id, dist, node.label))
        distances.sort(key=lambda x: x[1])
        return distances[:n]
    
    def find_conflicts(self, threshold: float = 1.5) -> List[Tuple[int, int, float]]:
        """Находит конфликтные пары (расстояние > threshold)."""
        conflicts = []
        for edge in self.edges:
            dist = self.get_distance(edge.from_id, edge.to_id)
            if dist > threshold:
                conflicts.append((edge.from_id, edge.to_id, dist))
        return conflicts
    
    def get_stats(self) -> Dict:
        """Возвращает статистику графа."""
        conflicts = self.find_conflicts()
        distances = []
        rhythms = []
        
        for edge in self.edges:
            dist = self.get_distance(edge.from_id, edge.to_id)
            distances.append(dist)
            if edge.dt > 0:
                rhythms.append(dist / edge.dt)
        
        return {
            'nodes': len(self.nodes),
            'edges': len(self.edges),
            'conflicts': len(conflicts),
            'avg_distance': sum(distances) / len(distances) if distances else 0,
            'max_distance': max(distances) if distances else 0,
            'min_distance': min(distances) if distances else 0,
            'avg_rhythm': sum(rhythms) / len(rhythms) if rhythms else 0,
            'rhythm_trit': quantize((sum(rhythms) / len(rhythms)) / self.radius) if rhythms else 0,
            'total_time': max((n.time for n in self.nodes.values()), default=0)
        }

# ============================================================================
# 4. RASHOMON (УНИВЕРСАЛЬНЫЙ)
# ============================================================================

def rashomon_analyze(
    interpretations: Dict[str, float],
    margin_threshold: float = 0.15,
    min_score: float = 0.3
) -> Dict:
    """Анализирует множество интерпретаций и определяет устойчивость ядра."""
    if not interpretations:
        return {'state': -1, 'top': None, 'margin': 0, 'reason': 'Нет интерпретаций'}

    sorted_items = sorted(interpretations.items(), key=lambda x: x[1], reverse=True)
    top_label, top_score = sorted_items[0]

    if len(sorted_items) == 1:
        margin = top_score
    else:
        margin = top_score - sorted_items[1][1]

    if top_score < min_score:
        state = -1
        reason = f"Лучшая оценка слишком низкая: {top_score:.2f}"
    elif margin >= margin_threshold:
        state = 1
        reason = f"Устойчивое ядро: отрыв {margin:.2f}"
    else:
        state = 0
        reason = f"Неопределённость: отрыв {margin:.2f}"

    return {
        'state': state,
        'top': top_label,
        'margin': margin,
        'top_score': top_score,
        'reason': reason,
        'all_scores': dict(sorted_items)
    }

# ============================================================================
# 5. ДЕТЕКЦИЯ КОНФЛИКТОВ
# ============================================================================

def detect_conflicts(text: str) -> Dict:
    """Обнаруживает явные противоречия в тексте."""
    lower = text.lower()
    patterns = [
        ['дешёвый', 'флагман'],
        ['дешёвый', 'лучший'],
        ['быстрый', 'дешёвый'],
        ['новый', 'дешёвый'],
        ['мало', 'много'],
        ['дешевле', 'дороже'],
        ['минимум', 'максимум'],
        ['хороший', 'дешёвый'],
    ]
    for a, b in patterns:
        if a in lower and b in lower:
            return {'conflict': True, 'a': a, 'b': b}
    return {'conflict': False}

# ============================================================================
# 6. МАТРИЧНЫЙ АНАЛИЗ СОГЛАСИЯ
# ============================================================================

def candidate_agreement_matrix(vectors: List[Tuple[int, ...]], names: List[str]) -> Dict:
    """Анализирует согласие между кандидатами по осям."""
    pairs = []
    for i in range(len(vectors)):
        for j in range(i + 1, len(vectors)):
            diff = ternary_diff(vectors[i], vectors[j])
            conflict_axes = [idx for idx, d in enumerate(diff) if abs(d) >= 2]
            pairs.append({
                'pair': (names[i], names[j]),
                'diff': diff,
                'conflict_axes': conflict_axes,
                'disagreement': bool(conflict_axes)
            })
    return {'pairs': pairs, 'has_disagreement': any(p['disagreement'] for p in pairs)}

# ============================================================================
# 7. ГЕНЕРАЦИЯ КАНДИДАТОВ
# ============================================================================

def generate_candidates(query: str, graph: HyperbolicGraph) -> Dict[str, Dict]:
    """Генерирует кандидатов на основе гиперболического графа."""
    lower = query.lower()
    candidates = {}

    # Преобразуем запрос в гиперболический вектор
    query_metrics = [
        0.7 if any(w in lower for w in ['телефон', 'смартфон', 'ноутбук', 'компьютер']) else 0.3,
        0.7 if any(w in lower for w in ['камера', 'цена', 'память', 'скорость']) else 0.3,
        0.7 if any(w in lower for w in ['до', 'бюджет', 'хочу', 'нужен']) else 0.3
    ]
    query_vec = hyperbolic_vector(query_metrics, graph.radius)

    # Находим ближайшие узлы в графе
    nearest = graph.nearest_nodes(query_vec, 3)

    for node_id, dist, label in nearest:
        if dist < float('inf'):
            z_match = max(0, 1 - dist / graph.radius)
            candidates[label] = {
                'z_match': z_match,
                'efficiency': 0.7 + 0.2 * (1 - dist / graph.radius),
                'reliability': 0.8,
                'tempo': 0.6,
                'strategic': 0.7,
                'consensus_opinions': [z_match, z_match * 0.9, z_match * 0.95],
                'graph_node': node_id,
                'graph_distance': dist
            }

    # Если кандидатов нет, добавляем общий ответ
    if not candidates:
        candidates['Общий ответ'] = {
            'z_match': 0.5,
            'efficiency': 0.5,
            'reliability': 0.6,
            'tempo': 0.5,
            'strategic': 0.5,
            'consensus_opinions': [0.5, 0.55, 0.45],
            'graph_node': None,
            'graph_distance': float('inf')
        }

    return candidates

# ============================================================================
# 8. ПОЛНЫЙ ЦИКЛ BLOCK X
# ============================================================================

class DecisionMode(Enum):
    DIRECT = "Direct Answer Mode"
    CLARIFICATION = "Clarification Mode"
    PROTECTION = "Protection Mode"

@dataclass
class BlockXResult:
    """Результат полного цикла Block X."""
    query: str
    graph_stats: Dict
    l3_in: int
    l3_in_reason: str
    l3_in_top: Optional[str]
    candidates: Dict[str, Dict]
    nearest_nodes: List[Tuple[int, float, str]]
    rashomon_2_results: Dict
    best_candidate: Optional[str]
    l3_out: int
    l3_out_reason: str
    delta_l3: int
    rc: int
    decoder_mode: str
    efficiency: Dict[str, Any]
    trajectory: List[Dict]
    rhythm_changes: List[Dict]
    graph_conflicts: List[Tuple[int, int, float]]
    final_decision: str
    log: Dict[str, Any] = field(default_factory=dict)
    rollback_needed: bool = False
    rollback_reason: Optional[str] = None

def run_block_x(
    query: str,
    input_interpretations: Dict[str, float],
    graph: HyperbolicGraph,
    margin_threshold: float = 0.15,
    risk: float = 0.1,
    risk_critical: float = 0.8
) -> BlockXResult:
    """
    Полный цикл Block X с гиперболическим графом и динамикой.
    """
    log = {}
    rollback_needed = False
    rollback_reason = None

    # --- ГРАФ: СТАТИСТИКА ---
    graph_stats = graph.get_stats()
    log['graph_stats'] = graph_stats

    # --- ШАГ 1: ДЕТЕКЦИЯ КОНФЛИКТОВ ---
    conflict = detect_conflicts(query)
    if conflict['conflict']:
        return BlockXResult(
            query=query,
            graph_stats=graph_stats,
            l3_in=-1,
            l3_in_reason=f"Конфликт: {conflict['a']} vs {conflict['b']}",
            l3_in_top=None,
            candidates={},
            nearest_nodes=[],
            rashomon_2_results={},
            best_candidate=None,
            l3_out=-1,
            l3_out_reason="Прервано на входе",
            delta_l3=0,
            rc=-1,
            decoder_mode="Protection Mode",
            efficiency={'efficiency_trit': -1, 'explanation': 'Прервано'},
            trajectory=[],
            rhythm_changes=[],
            graph_conflicts=[],
            final_decision="REJECT (конфликт)",
            log=log,
            rollback_needed=True,
            rollback_reason="CONFLICT_DETECTED"
        )

    # --- ШАГ 2: L3_in ---
    l3_in_result = rashomon_analyze(input_interpretations, margin_threshold)
    l3_in = l3_in_result['state']
    log['l3_in'] = l3_in_result

    if l3_in == -1:
        return BlockXResult(
            query=query,
            graph_stats=graph_stats,
            l3_in=-1,
            l3_in_reason=l3_in_result['reason'],
            l3_in_top=l3_in_result['top'],
            candidates={},
            nearest_nodes=[],
            rashomon_2_results={},
            best_candidate=None,
            l3_out=-1,
            l3_out_reason="Нет интерпретаций",
            delta_l3=0,
            rc=-1,
            decoder_mode="Protection Mode",
            efficiency={'efficiency_trit': -1, 'explanation': 'Нет интерпретаций'},
            trajectory=[],
            rhythm_changes=[],
            graph_conflicts=[],
            final_decision="REJECT (L3_in = -1)",
            log=log,
            rollback_needed=True,
            rollback_reason="NO_INTERPRETATIONS"
        )

    if l3_in == 0:
        return BlockXResult(
            query=query,
            graph_stats=graph_stats,
            l3_in=0,
            l3_in_reason=l3_in_result['reason'],
            l3_in_top=l3_in_result['top'],
            candidates={},
            nearest_nodes=[],
            rashomon_2_results={},
            best_candidate=None,
            l3_out=0,
            l3_out_reason="Неопределённость на входе",
            delta_l3=0,
            rc=0,
            decoder_mode="Clarification Mode",
            efficiency={'efficiency_trit': 0, 'explanation': 'Неопределённость'},
            trajectory=[],
            rhythm_changes=[],
            graph_conflicts=[],
            final_decision="CLARIFY (L3_in = 0)",
            log=log,
            rollback_needed=False
        )

    # --- ШАГ 3: ГЕНЕРАЦИЯ КАНДИДАТОВ ---
    candidates = generate_candidates(query, graph)
    log['candidates'] = candidates

    # --- ШАГ 4: БЛИЖАЙШИЕ УЗЛЫ ---
    query_metrics = [
        0.7 if any(w in query.lower() for w in ['телефон', 'смартфон', 'ноутбук', 'компьютер']) else 0.3,
        0.7 if any(w in query.lower() for w in ['камера', 'цена', 'память', 'скорость']) else 0.3,
        0.7 if any(w in query.lower() for w in ['до', 'бюджет', 'хочу', 'нужен']) else 0.3
    ]
    query_vec = hyperbolic_vector(query_metrics, graph.radius)
    nearest_nodes = graph.nearest_nodes(query_vec, 3)
    log['nearest_nodes'] = nearest_nodes

    # --- ШАГ 5: ТРАЕКТОРИЯ И РИТМ ---
    node_ids = [node_id for node_id, _, _ in nearest_nodes if node_id is not None]
    if len(node_ids) >= 2:
        trajectory = graph.get_trajectory_rhythm(node_ids)
        rhythm_changes = graph.detect_rhythm_changes(node_ids)
    else:
        trajectory = []
        rhythm_changes = []
    log['trajectory'] = trajectory
    log['rhythm_changes'] = rhythm_changes

    # --- ШАГ 6: ВЕКТОРЫ РЕШЕНИЯ ---
    solution_vectors = {}
    for name, data in candidates.items():
        vec = (
            quantize(data.get('z_match', 0.5)),
            quantize(data.get('efficiency', 0.5)),
            quantize(data.get('reliability', 0.5)),
            quantize(data.get('tempo', 0.5)),
            quantize(data.get('strategic', 0.5))
        )
        solution_vectors[name] = vec
    log['solution_vectors'] = solution_vectors

    # --- ШАГ 7: МАТРИЧНЫЙ АНАЛИЗ ---
    names = list(solution_vectors.keys())
    vectors = [solution_vectors[n] for n in names]
    matrix_result = candidate_agreement_matrix(vectors, names)
    log['matrix_analysis'] = matrix_result

    # --- ШАГ 8: ГРАФ КОНФЛИКТЫ ---
    graph_conflicts = graph.find_conflicts()
    log['graph_conflicts'] = graph_conflicts
    has_graph_conflict = len(graph_conflicts) > 0

    # --- ШАГ 9: RASHOMON-2 ---
    rashomon_2_results = {}
    for name, data in candidates.items():
        consensus = sum(data.get('consensus_opinions', [0.5])) / len(data.get('consensus_opinions', [0.5]))
        disagreement = matrix_result['has_disagreement'] or has_graph_conflict
        if disagreement:
            decision_score = 0
            l3_decision = 0
        else:
            decision_score = (data.get('z_match', 0.5) * data.get('efficiency', 0.5) *
                            data.get('reliability', 0.5) * consensus) ** (1/4)
            l3_decision = quantize(decision_score)
        rashomon_2_results[name] = {
            'Decision_score': decision_score,
            'L3_decision': l3_decision,
            'Disagreement': disagreement,
            'Consensus': consensus
        }
    log['rashomon_2'] = rashomon_2_results

    # --- ШАГ 10: ВЫБОР ЛУЧШЕГО ---
    eligible = [k for k, v in rashomon_2_results.items() if v['L3_decision'] != -1]
    if eligible:
        best_candidate = max(eligible, key=lambda k: rashomon_2_results[k]['Decision_score'])
        best_result = rashomon_2_results[best_candidate]
    else:
        best_candidate = None
        best_result = None
    log['best_candidate'] = best_candidate

    # --- ШАГ 11: L3_out ---
    if risk > risk_critical:
        l3_out = -1
        l3_out_reason = "Критический риск"
        rollback_needed = True
        rollback_reason = "RISK_CRITICAL"
    elif best_candidate is None:
        l3_out = -1
        l3_out_reason = "Нет подходящего кандидата"
        rollback_needed = True
        rollback_reason = "NO_CANDIDATE"
    else:
        l3_out = best_result['L3_decision']
        l3_out_reason = f"Выбран кандидат: {best_candidate}"
    log['l3_out'] = l3_out

    # --- ШАГ 12: ΔL3 ---
    delta_l3 = l3_out - l3_in
    log['delta_l3'] = delta_l3

    # --- ШАГ 13: Rc ---
    if best_candidate and best_candidate in solution_vectors:
        rc = ternary_fold(solution_vectors[best_candidate])
    else:
        rc = -1
    log['rc'] = rc

    # --- ШАГ 14: ДЕКОДЕР ---
    if l3_out == 1:
        decoder_mode = DecisionMode.DIRECT.value
        final_decision = "PASS (Direct Answer)"
    elif l3_out == 0:
        decoder_mode = DecisionMode.CLARIFICATION.value
        final_decision = "CLARIFY (L3_out = 0)"
    else:
        decoder_mode = DecisionMode.PROTECTION.value
        final_decision = "REJECT (L3_out = -1)"
    log['decoder_mode'] = decoder_mode

    # --- ШАГ 15: ЭФФЕКТИВНОСТЬ ---
    if best_candidate:
        value = best_result['Decision_score']
        cost = 0.3 + (1 - best_result['Consensus']) * 0.3
        efficiency = value / (cost + 0.001)
        efficiency_trit = quantize(efficiency)
        if efficiency_trit == 1:
            eff_explanation = "✅ Высокая эффективность"
        elif efficiency_trit == 0:
            eff_explanation = "⚖️ Средняя эффективность"
        else:
            eff_explanation = "❌ Низкая эффективность"
    else:
        value = 0
        cost = 1
        efficiency = 0
        efficiency_trit = -1
        eff_explanation = "Нет кандидата"

    efficiency_result = {
        'value': value,
        'cost': cost,
        'efficiency': efficiency,
        'efficiency_trit': efficiency_trit,
        'explanation': eff_explanation
    }
    log['efficiency'] = efficiency_result

    # --- ШАГ 16: ROLLBACK ---
    if rollback_needed:
        log['rollback'] = {'needed': True, 'reason': rollback_reason}

    return BlockXResult(
        query=query,
        graph_stats=graph_stats,
        l3_in=l3_in,
        l3_in_reason=l3_in_result['reason'],
        l3_in_top=l3_in_result['top'],
        candidates=candidates,
        nearest_nodes=nearest_nodes,
        rashomon_2_results=rashomon_2_results,
        best_candidate=best_candidate,
        l3_out=l3_out,
        l3_out_reason=l3_out_reason,
        delta_l3=delta_l3,
        rc=rc,
        decoder_mode=decoder_mode,
        efficiency=efficiency_result,
        trajectory=trajectory,
        rhythm_changes=rhythm_changes,
        graph_conflicts=graph_conflicts,
        final_decision=final_decision,
        log=log,
        rollback_needed=rollback_needed,
        rollback_reason=rollback_reason
    )

# ============================================================================
# 9. ВИЗУАЛИЗАЦИЯ РЕЗУЛЬТАТА
# ============================================================================

def print_result(result: BlockXResult) -> None:
    """Красиво выводит результат полного цикла."""
    print("=" * 70)
    print("ПОЛНЫЙ BLOCK X v6.3.2 (ГРАФЫ + ЛОБАЧЕВСКИЙ + ДИНАМИКА)")
    print("=" * 70)

    print(f"\n📝 ЗАПРОС: {result.query}")

    print(f"\n📊 ГРАФ:")
    print(f"   Узлов: {result.graph_stats['nodes']}")
    print(f"   Рёбер: {result.graph_stats['edges']}")
    print(f"   Конфликтов: {result.graph_stats['conflicts']}")
    print(f"   Среднее расстояние: {result.graph_stats['avg_distance']:.3f}")
    print(f"   Средний ритм: {result.graph_stats['avg_rhythm']:.3f}")
    print(f"   Ритм (трит): {result.graph_stats['rhythm_trit']}")

    if result.nearest_nodes:
        print(f"\n📍 БЛИЖАЙШИЕ УЗЛЫ:")
        for node_id, dist, label in result.nearest_nodes[:3]:
            print(f"   {label} (ID={node_id}): расстояние {dist:.3f}")

    if result.trajectory:
        print(f"\n🗺️ ТРАЕКТОРИЯ:")
        for step in result.trajectory:
            print(f"   {step['from']} → {step['to']}: путь={step['path']:.3f}, "
                  f"время={step['time']:.3f}, ритм={step['rhythm_trit']}")

    if result.rhythm_changes:
        print(f"\n🔄 ИЗМЕНЕНИЯ РИТМА:")
        for change in result.rhythm_changes:
            print(f"   В точке {change['index']}: изменение={change['rhythm_change']:.3f}")

    print(f"\n🔵 L3_in: {result.l3_in} ({result.l3_in_reason})")
    if result.l3_in_top:
        print(f"   Лучшая интерпретация: {result.l3_in_top}")

    if result.candidates:
        print(f"\n📋 КАНДИДАТЫ ({len(result.candidates)}):")
        for name, data in result.candidates.items():
            score = result.rashomon_2_results.get(name, {}).get('Decision_score', 0)
            l3 = result.rashomon_2_results.get(name, {}).get('L3_decision', 0)
            graph_dist = data.get('graph_distance', float('inf'))
            print(f"   {name}: score={score:.3f}, L3={l3}, dist={graph_dist:.3f}")

    if result.best_candidate:
        print(f"\n🏆 ЛУЧШИЙ КАНДИДАТ: {result.best_candidate}")

    print(f"\n🔴 L3_out: {result.l3_out} ({result.l3_out_reason})")
    print(f"📊 ΔL3: {result.delta_l3}")
    print(f"🌀 Rc: {result.rc}")
    print(f"🎯 ДЕКОДЕР: {result.decoder_mode}")

    print(f"\n⚡ ЭФФЕКТИВНОСТЬ:")
    print(f"   {result.efficiency['explanation']}")
    print(f"   Значение: {result.efficiency['efficiency']:.3f}")
    print(f"   Трит: {result.efficiency['efficiency_trit']}")

    if result.graph_conflicts:
        print(f"\n⚠️ КОНФЛИКТЫ В ГРАФЕ ({len(result.graph_conflicts)}):")
        for id1, id2, dist in result.graph_conflicts[:3]:
            print(f"   Узел {id1} ↔ Узел {id2}: расстояние {dist:.3f}")

    if result.rollback_needed:
        print(f"\n⚠️ ROLLBACK: {result.rollback_reason}")

    print(f"\n🎯 ФИНАЛЬНОЕ РЕШЕНИЕ: {result.final_decision}")

    print("\n" + "-" * 70)

# ============================================================================
# 10. ДЕМОНСТРАЦИЯ
# ============================================================================

if __name__ == "__main__":
    # Создаём гиперболический граф с динамикой
    graph = HyperbolicGraph(radius=1.5)

    # Добавляем узлы (координаты в гиперболическом шаре)
    graph.add_node(1, [0.8, 0.2, 0.1], "Смартфон с камерой", time=0.0, value=0.8)
    graph.add_node(2, [0.1, 0.9, 0.3], "Смартфон с автономностью", time=1.0, value=0.7)
    graph.add_node(3, [0.3, 0.1, 0.8], "Смартфон для игр", time=2.0, value=0.6)
    graph.add_node(4, [0.9, 0.8, 0.2], "Смартфон премиум", time=3.0, value=0.9)
    graph.add_node(5, [0.2, 0.3, 0.9], "Смартфон бюджетный", time=4.0, value=0.4)

    # Добавляем рёбра с временными шагами
    graph.add_edge(1, 2, weight=1.0, dt=1.0)
    graph.add_edge(1, 4, weight=1.0, dt=2.0)
    graph.add_edge(2, 5, weight=1.0, dt=1.0)
    graph.add_edge(3, 5, weight=1.0, dt=1.0)
    graph.add_edge(4, 1, weight=1.0, dt=1.0)

    # Тестовый запрос
    query = "хочу хороший телефон с камерой и автономностью до 600 евро"

    input_interpretations = {
        "Телефон с камерой и автономностью": 0.9,
        "Телефон с камерой и хорошим экраном": 0.7,
        "Телефон с автономностью и ценой до 600": 0.6
    }

    result = run_block_x(
        query=query,
        input_interpretations=input_interpretations,
        graph=graph,
        margin_threshold=0.15,
        risk=0.1
    )

    print_result(result)

    print("\n📋 ПОЛНЫЙ ЛОГ (ключевые этапы):")
    for key in ['l3_in', 'best_candidate', 'l3_out', 'delta_l3', 'rc', 'decoder_mode', 'efficiency']:
        if key in result.log:
            print(f"  {key}: {result.log[key]}")

    print("\n✅ Демонстрация завершена")
