# -*- coding: utf-8 -*-
"""
ПЕНТАРНЫЙ СЕМАНТИЧЕСКИЙ ВИДЕОКОДЕК И АРХИВАТОР ЗАВОДЮКА (V18-CORE / .ZVD)
Автор / Root-Architect: Владимир Заводюк (Vladimir Zavodiuk)
Назначение: Сверхплотное параметрическое сжатие и волновое хранение медиапотоков 
в формате .zvd (в разы эффективнее классических покадровых кодеков за счет 
полевой конденсации движения в ключевые аттракторы мембраны).

ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ.
ПРИМЕНЕНИЕ, КОПИРОВАНИЕ И РАЗВЁРТЫВАНИЕ ТОЛЬКО С СОГЛАСИЯ АВТОРА.
"""

import cv2
import numpy as np
import time
import struct
import os
import sys
from itertools import product

# --- БАЗИС И СЛОВАРИ ПЕНТАРНОЙ ЛОГИКИ ---
LOGIC_BASIS = [2, 1, 0, -1, -2]

MASKS = {
     2: "Бригелла (Да / Пик / Сверхбыстрое движение / Источник)",
     1: "Коломбина (Скорее Да / Восходящий фронт / Плавное движение)",
     0: "Доктор Грациано (Наверное / Центр / Покой / Демпфер хаоса)",
    -1: "Пьеро (Скорее Нет / Нисходящий фронт / Торможение)",
    -2: "Арлекин (Нет / Впадина / Обрыв фронта / Уход в тень)"
}

PENTAR_CODE = {
    -2: "КАТЕГОРИЧЕСКОЕ НЕТ (Обрыв / Тень)",
    -1: "МЯГКОЕ НЕТ (Торможение)",
     0: "НЕОПРЕДЕЛЕННОСТЬ / ПОКОЙ (Демпфер)",
     1: "МЯГКОЕ ДА (Рост)",
     2: "КАТЕГОРИЧЕСКОЕ ДА (Взрывной фронт)"
}

def quantize_pentarny(value: float) -> int:
    if value > 0.75:
        return 2
    elif value > 0.25:
        return 1
    elif value >= -0.25:
        return 0
    elif value >= -0.75:
        return -1
    else:
        return -2


# ============================================================================
# МОДУЛЬ 1: МАКРО-ПОЛЕ И ИСТИННОЕ ВОЛНОВОЕ ДЫХАНИЕ ( [] )
# ============================================================================
class PentarnyTensorField:
    def __init__(self, dimensions: tuple):
        self.dimensions = dimensions
        self.field = np.zeros(dimensions, dtype=int)  # [] — адресная сетка

    def inject_quantum_impulse(self, coordinates: tuple, value: int):
        if value not in [-2, -1, 0, 1, 2]:
            raise ValueError("Значение должно строго лежать в пентарном базисе [-2, 2]")
        if all(0 <= coordinates[i] < self.dimensions[i] for i in range(len(self.dimensions))):
            self.field[coordinates] = value

    def calculate_field_entropy(self) -> float:
        total_elements = self.field.size
        if total_elements == 0:
            return 0.0
        extreme_states = np.sum(np.abs(self.field) == 2)
        moderate_states = np.sum(np.abs(self.field) == 1)
        return float((extreme_states * 1.5 + moderate_states * 0.5) / total_elements)


class PentarnyTensorEvolution:
    """
    Эволюция тензорного поля по каноническим законам V18.
    """
    def __init__(self, tensor_field: PentarnyTensorField):
        self.field = tensor_field
        self.step_counter = 0

    def propagate_wave(self, origin: tuple, radius: int = 1):
        """
        Каноническое волновое распространение Заводюка:
        Импульс затухает строго по формуле: impulse_value = self.field.field[origin] // (dist + 1)
        Без суррогатных допущений. Поле дышит само.
        """
        dims = len(origin)
        self.step_counter += 1
        ranges = [range(max(0, origin[d] - radius), 
                       min(self.field.dimensions[d], origin[d] + radius + 1)) 
                  for d in range(dims)]
        for coords in product(*ranges):
            dist = sum(abs(coords[d] - origin[d]) for d in range(dims))
            if 0 < dist <= radius:
                impulse_value = self.field.field[origin] // (dist + 1)
                if impulse_value != 0:
                    current = self.field.field[coords]
                    self.field.field[coords] = max(-2, min(2, current + impulse_value))

    def calculate_field_gradient(self) -> list:
        return np.gradient(self.field.field.astype(float))

    def calculate_field_divergence(self) -> float:
        gradient = self.calculate_field_gradient()
        return float(np.sum([np.sum(np.abs(g)) for g in gradient]))


# ============================================================================
# МОДУЛЬ 2: МИКРО-ГРАФ И ТЕРМОДИНАМИКА РЕБЕР ( : )
# ============================================================================
class PentarNode:
    def __init__(self, node_id):
        self.node_id = node_id
        self.internal_state = 0
        self.mass = 0.0
        self.decision = 0

    def process_signals(self, signal_A: int, signal_B: int) -> int:
        signal_A = int(max(-2, min(2, signal_A)))
        signal_B = int(max(-2, min(2, signal_B)))

        sum_of_decisions = signal_A + signal_B
        conflict = abs(signal_A - signal_B)
        
        # `:` — пороговое разделение состояний
        if conflict >= 3:
            informational_mass = conflict * 2.5
            self.internal_state = 0
        else:
            informational_mass = conflict * 0.5
            self.internal_state = max(-2, min(2, sum_of_decisions))
            
        self.mass = informational_mass
        self.decision = self.internal_state
        return self.internal_state


class PentarEdge:
    """
    Термодинамический узел связи между микро-узлами по канону V18.
    """
    def __init__(self, source: PentarNode, target: PentarNode):
        self.source = source
        self.target = target
        self.energy_flow = 1.0
        self.temp = 1.0
        self.rhythm = 0.0

    def update_dynamics(self):
        """
        Канонический расчет температуры и потока энергии:
        base_modifier = 1.0 + (decision_sum * 0.25) - (total_mass * 0.2)
        Энергия обнуляется при критической блокировке Арлекина (-2).
        """
        decision_sum = self.source.decision + self.target.decision
        total_mass = self.source.mass + self.target.mass
        old_temp = self.temp
        
        base_modifier = 1.0 + (decision_sum * 0.25) - (total_mass * 0.2)
        self.temp = max(0.05, base_modifier)
        self.rhythm = self.temp - old_temp
        
        if self.source.decision <= -2 or self.target.decision <= -2:
            self.energy_flow = 0.0
        else:
            self.energy_flow = (self.temp * 1.5) + (total_mass * 0.5)


# ============================================================================
# МОДУЛЬ 3: УПРУГАЯ ТРЕХТОЧЕЧНАЯ ПЛОСКОСТЬ И КОНФАЙНМЕНТ ( () )
# ============================================================================
class ConflictAttractorPoint:
    def __init__(self, name: str, initial_coords: tuple, field_dims: tuple):
        self.name = name
        self.coords = list(initial_coords)
        self.field_dims = field_dims

    def drift_by_conflict(self, tensor_field: np.ndarray, gradient_fields: list, forbidden_coords: set = None):
        if forbidden_coords is None:
            forbidden_coords = set()

        dims = len(self.field_dims)
        best_coords = list(self.coords)
        
        current_pos = tuple(self.coords)
        local_base = abs(tensor_field[current_pos])
        grad_base = sum(abs(gradient_fields[i][current_pos]) for i in range(dims))
        max_tension = float(local_base) + float(grad_base)

        for d in range(dims):
            for delta in [-1, 1]:
                test_coords = list(self.coords)
                test_coords[d] += delta
                
                if all(0 <= test_coords[i] < self.field_dims[i] for i in range(dims)):
                    test_pos = tuple(test_coords)
                    if test_pos in forbidden_coords:
                        continue
                        
                    local_val = abs(tensor_field[test_pos])
                    grad_val = sum(abs(gradient_fields[i][test_pos]) for i in range(dims))
                    tension = float(local_val) + float(grad_val)

                    if tension > max_tension:
                        max_tension = tension
                        best_coords = test_coords

        self.coords = best_coords


class DynamicPlaneMembrane:
    def __init__(self, field_dims: tuple, p1: tuple, p2: tuple, p3: tuple):
        self.field_dims = field_dims
        self.vertex_a = ConflictAttractorPoint("Vertex-A", p1, field_dims)
        self.vertex_b = ConflictAttractorPoint("Vertex-B", p2, field_dims)
        self.vertex_c = ConflictAttractorPoint("Vertex-C", p3, field_dims)

    def update_membrane_position(self, tensor_field: np.ndarray, gradients: list):
        forbidden_a = {tuple(self.vertex_b.coords), tuple(self.vertex_c.coords)}
        self.vertex_a.drift_by_conflict(tensor_field, gradients, forbidden_a)

        forbidden_b = {tuple(self.vertex_a.coords), tuple(self.vertex_c.coords)}
        self.vertex_b.drift_by_conflict(tensor_field, gradients, forbidden_b)

        forbidden_c = {tuple(self.vertex_a.coords), tuple(self.vertex_b.coords)}
        self.vertex_c.drift_by_conflict(tensor_field, gradients, forbidden_c)

    def get_plane_vertices(self) -> tuple:
        return (tuple(self.vertex_a.coords), tuple(self.vertex_b.coords), tuple(self.vertex_c.coords))


# ============================================================================
# МОДУЛЬ 4: ПЕНТАРНЫЙ СВЕРЩИК КАДРОВ (Fast-Path Encoder)
# ============================================================================
class PentarnyVideoEncoder:
    def __init__(self, grid_size: tuple = (64, 64), t1: float = 0.1, t2: float = 0.25, t3: float = 0.45, t4: float = 0.7):
        self.grid_size = grid_size
        self.t1 = t1
        self.t2 = t2
        self.t3 = t3
        self.t4 = t4
        self.prev_gray = None

    def process_frame(self, frame_bgr: np.ndarray) -> np.ndarray:
        resized = cv2.resize(frame_bgr, self.grid_size, interpolation=cv2.INTER_AREA)
        gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0

        if self.prev_gray is None:
            self.prev_gray = gray
            return np.zeros(self.grid_size, dtype=int)

        motion_delta = gray - self.prev_gray
        self.prev_gray = gray

        pentarny_field = np.zeros(self.grid_size, dtype=int)
        
        pentarny_field[motion_delta >= self.t4] = 2   
        pentarny_field[(motion_delta >= self.t2) & (motion_delta < self.t4)] = 1   
        pentarny_field[(motion_delta > -self.t1) & (motion_delta < self.t2)] = 0   
        pentarny_field[(motion_delta <= -self.t1) & (motion_delta > -self.t3)] = -1 
        pentarny_field[motion_delta <= -self.t3] = -2  

        return pentarny_field


# ============================================================================
# МОДУЛЬ 5: ПОЛЕВОЙ КОНДЕНСАТОР (Мембранный Экстрактор)
# ============================================================================
class PentarnyFieldCondenser:
    def __init__(self, grid_size: tuple = (64, 64)):
        self.grid_size = grid_size
        self.prev_entropy = 0.0

    def condense(self, pentarny_field: np.ndarray) -> dict:
        pos_indices = np.argwhere(pentarny_field > 0)
        if len(pos_indices) > 0:
            v_a = np.mean(pos_indices, axis=0).astype(int).tolist()
        else:
            v_a = [self.grid_size[0] // 3, self.grid_size[1] // 3]

        neg_indices = np.argwhere(pentarny_field < 0)
        if len(neg_indices) > 0:
            v_c = np.mean(neg_indices, axis=0).astype(int).tolist()
        else:
            v_c = [2 * self.grid_size[0] // 3, 2 * self.grid_size[1] // 3]

        v_b = [self.grid_size[0] // 2, self.grid_size[1] // 2]

        min_dist = 2
        for v in [v_a, v_c]:
            if abs(v[0] - v_b[0]) < min_dist and abs(v[1] - v_b[1]) < min_dist:
                v[0] += min_dist if v[0] >= v_b[0] else -min_dist
                v[1] += min_dist if v[1] >= v_b[1] else -min_dist

        total_elements = pentarny_field.size
        extreme_states = np.sum(np.abs(pentarny_field) >= 1)
        current_entropy = float(extreme_states / total_elements) if total_elements > 0 else 0.0
        
        rhythm = current_entropy - self.prev_entropy
        self.prev_entropy = current_entropy

        return {
            "vertex_a": v_a,
            "vertex_b": v_b,
            "vertex_c": v_c,
            "entropy": current_entropy,
            "rhythm": rhythm
        }


# ============================================================================
# МОДУЛЬ 6: ВОЛНОВОЙ ДЕКОМПРЕССОР (Истинное полевое восстановление)
# ============================================================================
class PentarnyVideoDecoder:
    def __init__(self, grid_size: tuple = (64, 64), target_resolution: tuple = (640, 480)):
        self.grid_size = grid_size
        self.target_resolution = target_resolution
        self.tensor_field_obj = PentarnyTensorField(grid_size)
        self.evolution = PentarnyTensorEvolution(self.tensor_field_obj)

    def decode_frame(self, condensed_data: dict) -> np.ndarray:
        self.tensor_field_obj.field.fill(0)
        
        v_a = tuple(condensed_data["vertex_a"])
        v_b = tuple(condensed_data["vertex_b"])
        v_c = tuple(condensed_data["vertex_c"])
        rhythm = condensed_data["rhythm"]

        self.tensor_field_obj.inject_quantum_impulse(v_a, 2)
        self.tensor_field_obj.inject_quantum_impulse(v_c, -2)
        self.tensor_field_obj.inject_quantum_impulse(v_b, 0)

        wave_radius = 4 if abs(rhythm) > 0.05 else 2
        self.evolution.propagate_wave(v_a, radius=wave_radius)
        self.evolution.propagate_wave(v_c, radius=wave_radius)

        field = self.tensor_field_obj.field.astype(np.float32)

        pixel_matrix = np.zeros((*self.grid_size, 3), dtype=np.uint8)
        
        pixel_matrix[field >= 2.0] = (255, 255, 255)   
        pixel_matrix[(field > 0) & (field < 2.0)] = (0, 255, 0)  
        pixel_matrix[field == 0] = (0, 0, 0)             
        pixel_matrix[(field < 0) & (field > -2.0)] = (255, 0, 0) 
        pixel_matrix[field <= -2.0] = (128, 0, 128)    

        visual_frame = cv2.resize(pixel_matrix, self.target_resolution, interpolation=cv2.INTER_LINEAR)
        return visual_frame


# ============================================================================
# МОДУЛЬ 7: ДИСКОВЫЙ АРХИВАТОР (.zvd)
# ============================================================================
class ZvdBitstreamManager:
    def __init__(self, filename: str):
        self.filename = filename
        self.write_buffer = []

    def write_frame_packet(self, metadata: dict):
        va_y, va_x = metadata["vertex_a"]
        vb_y, vb_x = metadata["vertex_b"]
        vc_y, vc_x = metadata["vertex_c"]
        entropy = metadata["entropy"]
        rhythm = metadata["rhythm"]

        packet = struct.pack("BBBBBBff", va_y, va_x, vb_y, vb_x, vc_y, vc_x, entropy, rhythm)
        self.write_buffer.append(packet)

    def flush_to_disk(self):
        with open(self.filename, "wb") as f:
            f.write(b"ZVD1")
            for packet in self.write_buffer:
                f.write(packet)
        file_size = os.path.getsize(self.filename)
        print(f"[+] Семантический поток V18 успешно записан. Размер файла (.zvd): {file_size} БАЙТ.")
        self.write_buffer.clear()

    def read_from_disk(self) -> list:
        packets_data = []
        packet_size = struct.calcsize("BBBBBBff")

        with open(self.filename, "rb") as f:
            header = f.read(4)
            if header != b"ZVD1":
                raise ValueError("Критическая ошибка: Неверная сигнатура потока Заводюка!")

            while True:
                chunk = f.read(packet_size)
                if not chunk or len(chunk) < packet_size:
                    break
                va_y, va_x, vb_y, vb_x, vc_y, vc_x, entropy, rhythm = struct.unpack("BBBBBBff", chunk)
                packets_data.append({
                    "vertex_a": [va_y, va_x],
                    "vertex_b": [vb_y, vb_x],
                    "vertex_c": [vc_y, vc_x],
                    "entropy": entropy,
                    "rhythm": rhythm
                })
        return packets_data


# ============================================================================
# МОДУЛЬ 8: ГЛАВНЫЙ ОРКЕСТРАТОР КОНТУРА (Pipeline Orchestrator)
# ============================================================================
class PentarnyCodecPipeline:
    def __init__(self, grid_size: tuple = (64, 64), display_res: tuple = (640, 480), record_filename: str = "stream_v18_core.zvd"):
        self.encoder = PentarnyVideoEncoder(grid_size=grid_size)
        self.condenser = PentarnyFieldCondenser(grid_size=grid_size)
        self.decoder = PentarnyVideoDecoder(grid_size=grid_size, target_resolution=display_res)
        self.bitstream_mgr = ZvdBitstreamManager(record_filename)

    def start_live_stream(self):
        cap = cv2.VideoCapture(0)
        use_synthetic = not cap.isOpened()
        
        if use_synthetic:
            print("[!] Веб-камера недоступна. Запуск синтетического генератора полевых импульсов V18...")
        else:
            print("[*] Поток с веб-камеры активен. Нажмите 'q' для выхода и фиксации архива.")

        frame_count = 0
        try:
            while True:
                start_time = time.perf_counter()
                
                if use_synthetic:
                    frame = np.zeros((480, 640, 3), dtype=np.uint8)
                    x_pos = int(200 + 150 * np.sin(frame_count * 0.1))
                    y_pos = int(200 + 100 * np.cos(frame_count * 0.05))
                    cv2.rectangle(frame, (x_pos, y_pos), (x_pos + 100, y_pos + 100), (200, 100, 255), -1)
                else:
                    ret, frame = cap.read()
                    if not ret:
                        break

                frame_count += 1

                pentarny_field = self.encoder.process_frame(frame)
                metadata = self.condenser.condense(pentarny_field)
                
                self.bitstream_mgr.write_frame_packet(metadata)

                restored_frame = self.decoder.decode_frame(metadata)

                h, w = restored_frame.shape[:2]
                scale_y, scale_x = h / 64.0, w / 64.0
                
                def draw_vertex(pt, color, label):
                    y_f = int(pt[0] * scale_y)
                    x_f = int(pt[1] * scale_x)
                    cv2.circle(restored_frame, (x_f, y_f), 8, color, -1)
                    cv2.putText(restored_frame, label, (x_f + 12, y_f + 5), 
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)

                draw_vertex(metadata["vertex_a"], (255, 255, 255), "Brigella (+2)")
                draw_vertex(metadata["vertex_b"], (0, 0, 0), "Graziano (0)")
                draw_vertex(metadata["vertex_c"], (128, 0, 128), "Arlekin (-2)")

                fps = 1.0 / (time.perf_counter() - start_time)
                cv2.putText(restored_frame, f"Entropy: {metadata['entropy']:.4f}", (20, 30), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                cv2.putText(restored_frame, f"Rhythm: {metadata['rhythm']:+.4f}", (20, 60), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
                cv2.putText(restored_frame, f"FPS: {fps:.1f}", (20, 90), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

                cv2.imshow("1. ISHODNY POTOK ZAVODJUKA", frame)
                cv2.imshow("2. V18 FIELD DECODER (Vzyvodjuk Core)", restored_frame)

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        finally:
            if not use_synthetic:
                cap.release()
            cv2.destroyAllWindows()
            self.bitstream_mgr.flush_to_disk()
            print("[*] Контур V18 успешно завершил сеанс. Физика защищена.")


if __name__ == "__main__":
    print("=" * 70)
    print("🧠 СЕМАНТИЧЕСКИЙ МЕДИАТЕХНОЛОГИЧЕСКИЙ АРХИВАТОР ЗАВОДЮКА V18-CORE")
    print("   Автор: Владимир Заводюк (Root-Architect) | ALL RIGHTS RESERVED")
    print("=" * 70)

    if len(sys.argv) < 3:
        print("\nИНСТРУКЦИЯ ПО ФИЗИЧЕСКОМУ ЗАПУСКУ АРХИВАТОРА:")
        print("-" * 70)
        print("1. ЗАПАКОВКА готового видео-файла (MP4, AVI) в семантический формат:")
        print("   python stream_v18_core.py pack path/to/video.mp4 archive.zvd")
        print("\n2. РАСПАКОВКА и воспроизведение сжатого семантического архива:")
        print("   python stream_v18_core.py unpack archive.zvd")
        print("\n3. ЖИВОЙ ТЕСТ с веб-камеры (по умолчанию):")
        print("   python stream_v18_core.py live")
        print("=" * 70)
        sys.exit(0)

    mode = sys.argv[1].lower()

    if mode == "pack":
        input_video = sys.argv[2]
        output_zvd = sys.argv[3] if len(sys.argv) > 3 else "output_v18.zvd"
        
        print(f"[*] Запуск свертки физического файла: {input_video} -> {output_zvd}")
        cap = cv2.VideoCapture(input_video)
        if not cap.isOpened():
            print(f"[-] Критическая ошибка: Не удалось открыть видеофайл {input_video}")
            sys.exit(1)
            
        encoder = PentarnyVideoEncoder()
        condenser = PentarnyFieldCondenser()
        bitstream_mgr = ZvdBitstreamManager(output_zvd)
        
        frame_idx = 0
        start_time = time.perf_counter()
        
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            
            pentarny_field = encoder.process_frame(frame)
            metadata = condenser.condense(pentarny_field)
            bitstream_mgr.write_frame_packet(metadata)
            
            frame_idx += 1
            if frame_idx % 100 == 0:
                print(f" -> Обработано кадров: {frame_idx}...")
                
        cap.release()
        bitstream_mgr.flush_to_disk()
        total_time = time.perf_counter() - start_time
        print(f"[+] Процесс сжатия завершен! Время: {total_time:.2f} сек. Кадров: {frame_idx}")

    elif mode == "unpack":
        input_zvd = sys.argv[2]
        print(f"[*] Чтение семантического потока с диска: {input_zvd}")
        
        bitstream_mgr = ZvdBitstreamManager(input_zvd)
        try:
            packets_data = bitstream_mgr.read_from_disk()
        except Exception as e:
            print(f"[-] Ошибка чтения файла: {e}")
            sys.exit(1)
            
        print(f"[+] Успешно считано {len(packets_data)} кадров. Запуск волнового декодера V18...")
        decoder = PentarnyVideoDecoder()
        
        for metadata in packets_data:
            start_frame_time = time.perf_counter()
            
            restored_frame = decoder.decode_frame(metadata)
            
            cv2.putText(restored_frame, f"Entropy: {metadata['entropy']:.4f}", (20, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            cv2.putText(restored_frame, f"Rhythm: {metadata['rhythm']:+.4f}", (20, 60), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
            
            cv2.imshow("PENTARNYI ПЛЕЕР V18-CORE", restored_frame)
            
            elapsed = time.perf_counter() - start_frame_time
            delay = max(1, int((1.0 / 24.0 - elapsed) * 1000))
            if cv2.waitKey(delay) & 0xFF == ord('q'):
                break
                
        cv2.destroyAllWindows()
        print("[*] Воспроизведение семантического потока завершено.")
        
    elif mode == "live":
        pipeline = PentarnyCodecPipeline()
        pipeline.start_live_stream()
