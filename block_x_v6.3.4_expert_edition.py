#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
================================================================================
block_x_v6.3.3_final.py — ПОЛНАЯ ВЕРСИЯ BLOCK X (ядро принятия решений)
================================================================================
Включает:
- Троичную логику (quantize, ternary_fold, ternary_diff)
- Геометрию Лобачевского (гиперболическое пространство с защитой границ)
- Гиперболический граф (узлы, рёбра, расстояния, траектории, ритм)
- Двойной Рашимон (вход и выход)
- Матричный анализ согласия
- Полный цикл принятия решений

Автор: Владимир Заводюк (Root-Architect)
Лицензия: MIT
Версия: 6.3.3
================================================================================
"""

import math
import time
from typing import List, Tuple, Dict, Optional, Any
from dataclasses import dataclass, field

# ============================================================
# КОНФИГУРАЦИОННЫЕ КОНСТАНТЫ
# ============================================================
BOUNDARY_SAFETY_FACTOR = 0.999  # [ИСПРАВЛЕНО] запас до границы шара
MAX_ARG_FOR_ACOSH = 1e15        # [ИСПРАВЛЕНО] защита от переполнения

# ============================================================
# 1. ТРОИЧНАЯ ЛОГИКА
# ============================================================

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
    """Поэлементная разность с проверкой размерности."""
    if len(vec_a) != len(vec_b):
        raise ValueError(f"Размерности не совпадают: {len(vec_a)} != {len(vec_b)}")
    return tuple(a - b for a, b in zip(vec_a, vec_b))

# ============================================================
# 2. ГИПЕРБОЛИЧЕСКАЯ ГЕОМЕТРИЯ (ЛОБАЧЕВСКИЙ)
# ============================================================

def hyperbolic_distance(vec_a: Tuple[float, ...], vec_b: Tuple[float, ...], radius: float = 1.0) -> float:
    """Гиперболическое расстояние в модели Пуанкаре с защитой границ."""
    if len(vec_a) != len(vec_b):
        raise ValueError(f"Размерности не совпадают: {len(vec_a)} != {len(vec_b)}")
    
    a = [v / radius for v in vec_a]
    b = [v / radius for v in vec_b]
    
    norm_a = math.sqrt(sum(ai**2 for ai in a))
    norm_b = math.sqrt(sum(bi**2 for bi in b))
    
    max_safe = BOUNDARY_SAFETY_FACTOR  # [ИСПРАВЛЕНО] используем константу
    if norm_a >= 1.0: norm_a = max_safe
    if norm_b >= 1.0: norm_b = max_safe
    
    dot = sum(ai * bi for ai, bi in zip(a, b))
    numerator = sum((ai - bi)**2 for ai, bi in zip(a, b))
    denominator = (1.0 - norm_a**2) * (1.0 - norm_b**2)
    
    if denominator <= 0:
        return float('inf')
    
    arg = 1.0 + 2.0 * numerator / denominator
    if arg < 1.0:
        arg = 1.0
    # [ИСПРАВЛЕНО] защита от переполнения: acosh не может принять слишком большой аргумент
    if arg > MAX_ARG_FOR_ACOSH:
        return float('inf')
        
    return radius * math.acosh(arg)

def hyperbolic_vector(metrics: List[float], radius: float = 1.0) -> Tuple[float, ...]:
    """Преобразует метрики в гиперболический вектор с ограничением нормы."""
    trits = [quantize(m) for m in metrics]
    norm = math.sqrt(sum(t**2 for t in trits))
    max_allowable = radius * BOUNDARY_SAFETY_FACTOR  # [ИСПРАВЛЕНО]
    if norm > max_allowable and norm > 0:
        trits = [t * max_allowable / norm for t in trits]
    return tuple(float(t) for t in trits)

# ============================================================
# 3. ГИПЕРБОЛИЧЕСКИЙ ГРАФ
# ============================================================

@dataclass
class HyperbolicNode:
    id: int
    coordinates: Tuple[float, ...]
    label: str
    time: float = 0.0
    value: float = 0.0

@dataclass
class HyperbolicEdge:
    from_id: int
    to_id: int
    weight: float = 1.0
    dt: float = 1.0

class HyperbolicGraph:
    def __init__(self, radius: float = 1.0, dimension: int = 3):
        self.radius = radius
        self.dimension = dimension
        self.nodes: Dict[int, HyperbolicNode] = {}
        self.edges: List[HyperbolicEdge] = []
    
    def add_node(self, node_id: int, coordinates: List[float], label: str = "", time: float = 0.0, value: float = 0.0):
        # Авто-корректировка размерности
        if len(coordinates) < self.dimension:
            coordinates = list(coordinates) + [0.0] * (self.dimension - len(coordinates))
        else:
            coordinates = coordinates[:self.dimension]
        
        norm = math.sqrt(sum(c**2 for c in coordinates))
        max_allowable = self.radius * BOUNDARY_SAFETY_FACTOR  # [ИСПРАВЛЕНО]
        if norm >= max_allowable and norm > 0:
            coordinates = [c * max_allowable / norm for c in coordinates]
            
        self.nodes[node_id] = HyperbolicNode(
            id=node_id,
            coordinates=tuple(float(c) for c in coordinates),
            label=label or f"Узел {node_id}",
            time=time,
            value=value
        )
    
    def add_edge(self, from_id: int, to_id: int, weight: float = 1.0, dt: float = 1.0):
        if from_id in self.nodes and to_id in self.nodes:
            self.edges.append(HyperbolicEdge(from_id, to_id, weight, dt))
    
    def get_distance(self, id1: int, id2: int) -> float:
        if id1 not in self.nodes or id2 not in self.nodes:
            return float('inf')
        return hyperbolic_distance(self.nodes[id1].coordinates, self.nodes[id2].coordinates, self.radius)
    
    def get_path_metrics(self, id1: int, id2: int) -> Dict:
        if id1 not in self.nodes or id2 not in self.nodes:
            return {'error': 'Узлы не найдены'}
        
        distance = self.get_distance(id1, id2)
        dt = abs(self.nodes[id2].time - self.nodes[id1].time)
        if dt == 0: dt = 0.001
        
        rhythm = distance / dt
        return {
            'path': distance,
            'time': dt,
            'rhythm': rhythm,
            'rhythm_trit': quantize(rhythm / self.radius),
            'path_trit': quantize(distance / self.radius)
        }
    
    def get_trajectory_rhythm(self, node_ids: List[int]) -> List[Dict]:
        results = []
        for i in range(len(node_ids) - 1):
            id1, id2 = node_ids[i], node_ids[i+1]
            metrics = self.get_path_metrics(id1, id2)
            results.append({'from': id1, 'to': id2, **metrics})
        return results
    
    def detect_rhythm_changes(self, node_ids: List[int], threshold: float = 0.5) -> List[Dict]:
        results, rhythms = [], []
        for i in range(len(node_ids) - 1):
            metrics = self.get_path_metrics(node_ids[i], node_ids[i+1])
            rhythms.append(metrics.get('rhythm', 0))
        
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
        distances = []
        for node_id, node in self.nodes.items():
            dist = hyperbolic_distance(vector, node.coordinates, self.radius)
            distances.append((node_id, dist, node.label))
        distances.sort(key=lambda x: x[1])
        return distances[:n]
    
    def find_conflicts(self, threshold: float = 1.5) -> List[Tuple[int, int, float]]:
        return [(e.from_id, e.to_id, self.get_distance(e.from_id, e.to_id)) 
                for e in self.edges if self.get_distance(e.from_id, e.to_id) > threshold]
    
    def get_stats(self) -> Dict:
        distances = [self.get_distance(e.from_id, e.to_id) for e in self.edges]
        rhythms = [d / e.dt for d, e in zip(distances, self.edges) if e.dt > 0]
        
        avg_dist = sum(distances) / len(distances) if distances else 0.0
        avg_rhythm = sum(rhythms) / len(rhythms) if rhythms else 0.0
        
        return {
            'nodes': len(self.nodes),
            'edges': len(self.edges),
            'conflicts': len(self.find_conflicts()),
            'avg_distance': avg_dist,
            'avg_rhythm': avg_rhythm,
            'rhythm_trit': quantize(avg_rhythm / self.radius) if rhythms else 0,
            'total_time': max((n.time for n in self.nodes.values()), default=0.0)
        }

# ============================================================
# 4. RASHOMON (ВХОД + ВЫХОД)
# ============================================================

def rashomon_analyze(interpretations: Dict[str, float], margin_threshold: float = 0.15, min_score: float = 0.3) -> Dict:
    if not interpretations:
        return {'state': -1, 'top': None, 'margin': 0, 'reason': 'Нет интерпретаций'}

    sorted_items = sorted(interpretations.items(), key=lambda x: x[1], reverse=True)
    top_label, top_score = sorted_items[0]
    margin = top_score - sorted_items[1][1] if len(sorted_items) > 1 else top_score

    if top_score < min_score:
        state, reason = -1, f"Лучшая оценка слишком низкая: {top_score:.2f}"
    elif margin >= margin_threshold:
        state, reason = 1, f"Устойчивое ядро: отрыв {margin:.2f}"
    else:
        state, reason = 0, f"Неопределённость: отрыв {margin:.2f}"

    return {'state': state, 'top': top_label, 'margin': margin, 'reason': reason}

def detect_conflicts(text: str) -> Dict:
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

# [ИСПРАВЛЕНО] Мягкая матрица согласия с total_discrepancy
def candidate_agreement_matrix(vectors: List[Tuple[int, ...]], names: List[str]) -> Dict:
    pairs = []
    for i in range(len(vectors)):
        for j in range(i + 1, len(vectors)):
            diff = ternary_diff(vectors[i], vectors[j])
            conflict_axes = [idx for idx, d in enumerate(diff) if abs(d) >= 2]
            total_discrepancy = sum(abs(d) for d in diff)  # [ИСПРАВЛЕНО] дополнительный индикатор
            pairs.append({
                'pair': (names[i], names[j]),
                'diff': diff,
                'conflict_axes': conflict_axes,
                'disagreement': bool(conflict_axes),
                'total_discrepancy': total_discrepancy
            })
    return {'pairs': pairs, 'has_disagreement': any(p['disagreement'] for p in pairs)}

# ============================================================
# 5. ГЕНЕРАЦИЯ КАНДИДАТОВ (С ЭВРИСТИКОЙ ЗАПРОСА)
# ============================================================

# [ИСПРАВЛЕНО] Эвристическое извлечение метрик из запроса
def query_to_metrics(query: str) -> List[float]:
    lower = query.lower()
    camera = 0.8 if any(w in lower for w in ['камер', 'фото', 'съёмк']) else 0.2
    battery = 0.8 if any(w in lower for w in ['автоном', 'батар', 'заряд']) else 0.2
    gaming = 0.8 if any(w in lower for w in ['игр', 'гейм', 'производит']) else 0.2
    price = 0.8 if any(w in lower for w in ['дешёв', 'бюджет', 'цена', 'евро', '600']) else 0.2
    return [camera, battery, gaming, price]

# [ИСПРАВЛЕНО] Генерация кандидатов с динамическим вектором запроса
def generate_candidates(query_vec: Tuple[float, ...], graph: HyperbolicGraph) -> Dict[str, Dict]:
    nearest = graph.nearest_nodes(query_vec, 3)
    
    candidates = {}
    for node_id, dist, label in nearest:
        if dist < float('inf'):
            z_match = max(0.0, 1.0 - dist / graph.radius)
            candidates[label] = {
                'z_match': z_match,
                'efficiency': 0.7 + 0.2 * z_match,
                'reliability': 0.8,
                'tempo': 0.6,
                'strategic': 0.7,
                'consensus_opinions': [z_match, z_match * 0.9, z_match * 0.95],
                'graph_node': node_id,
                'graph_distance': dist
            }
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

# ============================================================
# 6. ПОЛНЫЙ ЦИКЛ BLOCK X (run_block_x)
# ============================================================

@dataclass
class BlockXResult:
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
    risk: float = 0.0  # [ИСПРАВЛЕНО] добавим отображение итогового риска

# [ИСПРАВЛЕНО] Убрали параметр risk, теперь вычисляем динамически
def run_block_x(query: str, input_interpretations: Dict[str, float], graph: HyperbolicGraph,
                margin_threshold: float = 0.15, risk_critical: float = 0.8) -> BlockXResult:
    log = {}
    graph_stats = graph.get_stats()
    log['graph_stats'] = graph_stats

    # 1. Детекция конфликтов
    conflict = detect_conflicts(query)
    if conflict['conflict']:
        return BlockXResult(
            query=query, graph_stats=graph_stats, l3_in=-1,
            l3_in_reason=f"Конфликт: {conflict['a']} vs {conflict['b']}",
            l3_in_top=None, candidates={}, nearest_nodes=[],
            rashomon_2_results={}, best_candidate=None,
            l3_out=-1, l3_out_reason="Прервано на входе",
            delta_l3=0, rc=-1, decoder_mode="Protection Mode",
            efficiency={'efficiency_trit': -1, 'explanation': 'Прервано'},
            trajectory=[], rhythm_changes=[], graph_conflicts=[],
            final_decision="REJECT (конфликт)", log=log,
            rollback_needed=True, rollback_reason="CONFLICT_DETECTED", risk=1.0
        )

    # 2. L3_in (Rashomon-1)
    l3_in_result = rashomon_analyze(input_interpretations, margin_threshold)
    l3_in = l3_in_result['state']
    log['l3_in'] = l3_in_result

    if l3_in in (-1, 0):
        mode_name = "Protection Mode" if l3_in == -1 else "Clarification Mode"
        risk_value = 1.0 if l3_in == -1 else 0.5  # базовый риск до уточнения
        return BlockXResult(
            query=query, graph_stats=graph_stats, l3_in=l3_in,
            l3_in_reason=l3_in_result['reason'], l3_in_top=l3_in_result['top'],
            candidates={}, nearest_nodes=[], rashomon_2_results={},
            best_candidate=None, l3_out=l3_in, l3_out_reason=l3_in_result['reason'],
            delta_l3=0, rc=l3_in, decoder_mode=mode_name,
            efficiency={'efficiency_trit': l3_in, 'explanation': 'Остановлено'},
            trajectory=[], rhythm_changes=[], graph_conflicts=[],
            final_decision=f"REJECT/CLARIFY (L3_in = {l3_in})", log=log,
            rollback_needed=(l3_in == -1), rollback_reason="L3_IN_NON_POSITIVE", risk=risk_value
        )

    # 3. Генерация кандидатов с динамическим вектором запроса
    # [ИСПРАВЛЕНО] используем query_to_metrics и гиперболический вектор
    raw_metrics = query_to_metrics(query)
    # Приводим размерность к размерности графа
    if len(raw_metrics) < graph.dimension:
        raw_metrics.extend([0.2] * (graph.dimension - len(raw_metrics)))
    else:
        raw_metrics = raw_metrics[:graph.dimension]
    query_vec = hyperbolic_vector(raw_metrics, graph.radius)
    log['query_vec'] = query_vec

    candidates = generate_candidates(query_vec, graph)
    log['candidates'] = candidates

    # 4. Ближайшие узлы
    nearest_nodes = graph.nearest_nodes(query_vec, 3)
    log['nearest_nodes'] = nearest_nodes

    # 5. Траектория и ритм
    node_ids = [n_id for n_id, _, _ in nearest_nodes if n_id is not None]
    trajectory = graph.get_trajectory_rhythm(node_ids) if len(node_ids) >= 2 else []
    rhythm_changes = graph.detect_rhythm_changes(node_ids) if len(node_ids) >= 2 else []
    log['trajectory'] = trajectory
    log['rhythm_changes'] = rhythm_changes

    # 6. Векторы решений
    solution_vectors = {
        name: (quantize(data['z_match']), quantize(data['efficiency']),
               quantize(data['reliability']), quantize(data['tempo']), quantize(data['strategic']))
        for name, data in candidates.items()
    }
    log['solution_vectors'] = solution_vectors

    # 7. Матричный анализ согласия
    names = list(solution_vectors.keys())
    matrix_result = candidate_agreement_matrix([solution_vectors[n] for n in names], names)
    log['matrix_analysis'] = matrix_result

    # 8. Конфликты в графе
    graph_conflicts = graph.find_conflicts()
    log['graph_conflicts'] = graph_conflicts
    has_graph_conflict = len(graph_conflicts) > 0

    # [ИСПРАВЛЕНО] Динамический риск на основе margin и конфликтов
    base_risk = max(0.0, 1.0 - l3_in_result['margin'])  # чем меньше отрыв, тем выше риск
    if matrix_result['has_disagreement'] or has_graph_conflict:
        dynamic_risk = min(1.0, base_risk + 0.3)
    else:
        dynamic_risk = base_risk
    log['risk'] = dynamic_risk

    # 9. Rashomon-2
    rashomon_2_results = {}
    for name, data in candidates.items():
        consensus = sum(data['consensus_opinions']) / len(data['consensus_opinions'])
        disagreement = matrix_result['has_disagreement'] or has_graph_conflict
        if disagreement:
            decision_score, l3_decision = 0.0, 0
        else:
            decision_score = (data['z_match'] * data['efficiency'] * data['reliability'] * consensus) ** 0.25
            l3_decision = quantize(decision_score)
        rashomon_2_results[name] = {'Decision_score': decision_score, 'L3_decision': l3_decision,
                                    'Disagreement': disagreement, 'Consensus': consensus}
    log['rashomon_2'] = rashomon_2_results

    # 10. Выбор лучшего
    eligible = [k for k, v in rashomon_2_results.items() if v['L3_decision'] != -1]
    best_candidate = max(eligible, key=lambda k: rashomon_2_results[k]['Decision_score']) if eligible else None
    log['best_candidate'] = best_candidate

    # 11. L3_out и оценка риска
    if dynamic_risk > risk_critical:
        l3_out, l3_out_reason, rollback_needed, rollback_reason = -1, f"Критический риск ({dynamic_risk:.2f})", True, "RISK_CRITICAL"
    elif best_candidate is None:
        l3_out, l3_out_reason, rollback_needed, rollback_reason = -1, "Нет подходящего кандидата", True, "NO_CANDIDATE"
    else:
        l3_out = rashomon_2_results[best_candidate]['L3_decision']
        l3_out_reason = f"Выбран кандидат: {best_candidate}"
        rollback_needed, rollback_reason = False, None

    # 12. ΔL3, Rc, Decoder
    delta_l3 = l3_out - l3_in
    rc = ternary_fold(solution_vectors[best_candidate]) if best_candidate and best_candidate in solution_vectors else -1

    decoder_modes = {1: "Direct Answer Mode", 0: "Clarification Mode", -1: "Protection Mode"}
    decoder_mode = decoder_modes.get(l3_out, "Protection Mode")
    final_decisions = {1: "PASS (Direct Answer)", 0: "CLARIFY (L3_out = 0)", -1: "REJECT (L3_out = -1)"}
    final_decision = final_decisions.get(l3_out, "REJECT")

    # 13. Эффективность
    if best_candidate:
        value = rashomon_2_results[best_candidate]['Decision_score']
        cost = 0.3 + (1.0 - rashomon_2_results[best_candidate]['Consensus']) * 0.3
        efficiency = value / cost if cost > 0 else float('inf')
        efficiency_trit = quantize(efficiency)
        eff_expl = "✅ Высокая" if efficiency_trit == 1 else ("⚖️ Средняя" if efficiency_trit == 0 else "❌ Низкая")
    else:
        value, cost, efficiency, efficiency_trit, eff_expl = 0.0, 1.0, 0.0, -1, "Нет кандидата"

    efficiency_result = {'value': value, 'cost': cost, 'efficiency': efficiency,
                         'efficiency_trit': efficiency_trit, 'explanation': eff_expl}

    # 14. Лог
    log['l3_out'] = l3_out
    log['delta_l3'] = delta_l3
    log['rc'] = rc
    log['decoder_mode'] = decoder_mode
    log['efficiency'] = efficiency_result
    if rollback_needed:
        log['rollback'] = {'needed': True, 'reason': rollback_reason}

    return BlockXResult(
        query=query, graph_stats=graph_stats,
        l3_in=l3_in, l3_in_reason=l3_in_result['reason'], l3_in_top=l3_in_result['top'],
        candidates=candidates, nearest_nodes=nearest_nodes,
        rashomon_2_results=rashomon_2_results, best_candidate=best_candidate,
        l3_out=l3_out, l3_out_reason=l3_out_reason,
        delta_l3=delta_l3, rc=rc, decoder_mode=decoder_mode,
        efficiency=efficiency_result,
        trajectory=trajectory, rhythm_changes=rhythm_changes, graph_conflicts=graph_conflicts,
        final_decision=final_decision, log=log,
        rollback_needed=rollback_needed, rollback_reason=rollback_reason, risk=dynamic_risk
    )

# ============================================================
# 7. ДЕМОНСТРАЦИЯ
# ============================================================

if __name__ == "__main__":
    print("=" * 60)
    print("BLOCK X v6.3.3 — ИСПРАВЛЕННАЯ ВЕРСИЯ")
    print("=" * 60)

    # Создаём гиперболический граф (размерность 4 для новых метрик)
    graph = HyperbolicGraph(radius=1.5, dimension=4)
    graph.add_node(1, [0.8, 0.2, 0.1, 0.5], "Смартфон с камерой", time=0.0, value=0.8)
    graph.add_node(2, [0.1, 0.9, 0.3, 0.6], "Смартфон с автономностью", time=1.0, value=0.7)
    graph.add_node(3, [0.3, 0.1, 0.8, 0.2], "Смартфон для игр", time=2.0, value=0.6)
    graph.add_node(4, [0.9, 0.8, 0.2, 0.1], "Смартфон премиум", time=3.0, value=0.9)
    graph.add_node(5, [0.2, 0.3, 0.9, 0.8], "Смартфон бюджетный", time=4.0, value=0.4)
    graph.add_edge(1, 2, weight=1.0, dt=1.0)
    graph.add_edge(1, 4, weight=1.0, dt=2.0)
    graph.add_edge(2, 5, weight=1.0, dt=1.0)
    graph.add_edge(3, 5, weight=1.0, dt=1.0)

    query = "хочу хороший телефон с камерой и автономностью до 600 евро"
    input_interpretations = {
        "Телефон с камерой и автономностью": 0.9,
        "Телефон с камерой и хорошим экраном": 0.7,
        "Телефон с автономностью и ценой до 600": 0.6
    }

    result = run_block_x(query, input_interpretations, graph, margin_threshold=0.15, risk_critical=0.8)

    print(f"\n📝 ЗАПРОС: {result.query}")
    print(f"🔵 L3_in: {result.l3_in} ({result.l3_in_reason})")
    print(f"📉 Динамический риск: {result.risk:.2f}")
    print(f"🏆 Лучший кандидат: {result.best_candidate}")
    print(f"🔴 L3_out: {result.l3_out} ({result.l3_out_reason})")
    print(f"📊 ΔL3: {result.delta_l3}")
    print(f"🌀 Rc: {result.rc}")
    print(f"🎯 Декодер: {result.decoder_mode}")
    print(f"⚡ Эффективность: {result.efficiency['explanation']}")
    print(f"🎯 Финальное решение: {result.final_decision}")
    print("\n📊 Статистика графа:")
    for k, v in result.graph_stats.items():
        print(f"  {k}: {v}")
    print("\n✅ Демонстрация завершена.")
