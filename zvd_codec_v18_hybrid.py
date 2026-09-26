# -*- coding: utf-8 -*-
"""
ПЕНТАРНЫЙ ГИБРИДНЫЙ СЕМАНТИЧЕСКИЙ ВИДЕОКОДЕК И АРХИВАТОР ЗАВОДЮКА (V18-HYBRID / .ZVD)
Автор / Root-Architect: Владимир Заводюк (Vladimir Zavodiuk)
Назначение: Сверхплотное параметрическое сжатие, волновое хранение и гибридная 
текстурная реконструкция медиапотоков в формате .zvd (матрица 256x256).

ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ.
ПРИМЕНЕНИЕ, КОПИРОВАНИЕ И РАЗВЁРТЫВАНИЕ ТОЛЬКО С СОГЛАСИЯ АВТОРА.
"""

import cv2
import numpy as np
import time
import struct
import os
import sys
import zlib
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
# МОДУЛЬ 1: МАКРО-ПОЛЕ И ВОЛНОВОЕ ДЫХАНИЕ (256x256)
# ============================================================================
class PentarnyTensorField:
    def __init__(self, dimensions: tuple = (256, 256)):
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
    Эволюция тензорного поля по законам V18-Pro для сетки 256x256.
    """
    def __init__(self, tensor_field: PentarnyTensorField):
        self.field = tensor_field
        self.step_counter = 0

    def propagate_wave(self, origin: tuple, radius: int = 8):
        """
        Масштабированный волновой радиус для корректного покрытия энергопотока.
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

    def decode_wave_to_brightness(self) -> np.ndarray:
        """
        Мгновенный перевод дискретного пентарного поля [-2, 2] 
        в непрерывную физическую яркость [0, 255] для формирования скелета.
        """
        field_f = self.field.field.astype(np.float32)
        normalized = ((field_f + 2.0) / 4.0) * 255.0
        return np.clip(normalized, 0, 255).astype(np.uint8)


# ============================================================================
# МОДУЛЬ 2: МИКРО-ГРАФ, ТЕРМОДИНАМИКА И АДАПТИВНЫЙ ОСТАТОК
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
    def __init__(self, source: PentarNode, target: PentarNode):
        self.source = source
        self.target = target
        self.energy_flow = 1.0
        self.temp = 1.0
        self.rhythm = 0.0

    def update_dynamics(self):
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


class ZvdAdaptiveResidualEngine:
    """
    Движок вычисления остатка и адаптивного троично-пентарного квантования
    с учетом термодинамического ритма полевой энтропии.
    """
    def __init__(self, grid_size: tuple = (256, 256), base_threshold: int = 15):
        self.grid_size = grid_size
        self.base_threshold = base_threshold

    def calculate_and_quantize(self, real_frame: np.ndarray, v18_frame: np.ndarray, rhythm: float) -> bytes:
        residual = real_frame.astype(np.int16) - v18_frame.astype(np.int16)
        adaptive_threshold = self.base_threshold + int(abs(rhythm) * 50.0)
        adaptive_threshold = max(5, min(60, adaptive_threshold))
        
        quantized = np.where(np.abs(residual) < adaptive_threshold, 0, residual).astype(np.int16)
        nonzero_indices = np.argwhere(quantized != 0)
        nonzero_values = quantized[quantized != 0]
        
        count = len(nonzero_values)
        payload = struct.pack("I", count)
        
        for idx, val in zip(nonzero_indices, nonzero_values):
            payload += struct.pack("BBh", int(idx[0]), int(idx[1]), int(val))
            
        return zlib.compress(payload)

    def restore_and_blend(self, v18_frame: np.ndarray, compressed_residual: bytes) -> np.ndarray:
        if not compressed_residual:
            return v18_frame.copy()
            
        payload = zlib.decompress(compressed_residual)
        count = struct.unpack("I", payload[:4])[0]
        
        residual_matrix = np.zeros(self.grid_size, dtype=np.int16)
        offset = 4
        packet_size = struct.calcsize("BBh")
        
        for _ in range(count):
            if offset + packet_size > len(payload):
                break
            y, x, val = struct.unpack("BBh", payload[offset:offset+packet_size])
            residual_matrix[y, x] = val
            offset += packet_size
            
        blended_frame = v18_frame.astype(np.int16) + residual_matrix
        return np.clip(blended_frame, 0, 255).astype(np.uint8)


# ============================================================================
# МОДУЛЬ 4: КОДЕР, КОНДЕНСАТОР И ГИБРИДНЫЙ ДЕКОДЕР
# ============================================================================
class PentarnyVideoEncoder:
    def __init__(self, grid_size: tuple = (256, 256), t1: float = 0.05, t2: float = 0.15, t3: float = 0.30, t4: float = 0.55):
        self.grid_size = grid_size
        self.t1 = t1
        self.t2 = t2
        self.t3 = t3
        self.t4 = t4
        self.prev_gray = None

    def process_frame(self, frame_bgr: np.ndarray, rhythm_modulation: float = 0.0) -> np.ndarray:
        resized = cv2.resize(frame_bgr, self.grid_size, interpolation=cv2.INTER_AREA)
        gray = cv2.cvtColor(resized, cv2.COLOR_BGR2GRAY).astype(np.float32) / 255.0

        if self.prev_gray is None:
            self.prev_gray = gray
            return np.zeros(self.grid_size, dtype=int)

        mod = np.clip(rhythm_modulation, -0.05, 0.05)
        at1 = max(0.01, self.t1 + mod)
        at2 = max(0.05, self.t2 + mod)
        at3 = max(0.15, self.t3 + mod)
        at4 = max(0.30, self.t4 + mod)

        motion_delta = gray - self.prev_gray
        self.prev_gray = gray

        pentarny_field = np.zeros(self.grid_size, dtype=int)
        pentarny_field[motion_delta >= at4] = 2   
        pentarny_field[(motion_delta >= at2) & (motion_delta < at4)] = 1   
        pentarny_field[(motion_delta > -at1) & (motion_delta < at2)] = 0   
        pentarny_field[(motion_delta <= -at1) & (motion_delta > -at3)] = -1 
        pentarny_field[motion_delta <= -at3] = -2  

        return pentarny_field


class PentarnyFieldCondenser:
    def __init__(self, grid_size: tuple = (256, 256)):
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

        min_dist = 4
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


class PentarnyVideoDecoder:
    def __init__(self, grid_size: tuple = (256, 256), target_resolution: tuple = (640, 480)):
        self.grid_size = grid_size
        self.target_resolution = target_resolution
        self.tensor_field_obj = PentarnyTensorField(grid_size)
        self.evolution = PentarnyTensorEvolution(self.tensor_field_obj)
        self.residual_engine = ZvdAdaptiveResidualEngine(grid_size=grid_size)

    def decode_frame(self, metadata: dict) -> np.ndarray:
        self.tensor_field_obj.field.fill(0)
        
        v_a = tuple(metadata["vertex_a"])
        v_b = tuple(metadata["vertex_b"])
        v_c = tuple(metadata["vertex_c"])
        rhythm = metadata["rhythm"]
        compressed_residual = metadata.get("compressed_residual", b"")

        self.tensor_field_obj.inject_quantum_impulse(v_a, 2)
        self.tensor_field_obj.inject_quantum_impulse(v_c, -2)
        self.tensor_field_obj.inject_quantum_impulse(v_b, 0)

        wave_radius = 16 if abs(rhythm) > 0.05 else 8
        self.evolution.propagate_wave(v_a, radius=wave_radius)
        self.evolution.propagate_wave(v_c, radius=wave_radius)

        base_gray_256 = self.evolution.decode_wave_to_brightness()
        blended_256 = self.residual_engine.restore_and_blend(base_gray_256, compressed_residual)
        pixel_matrix = cv2.cvtColor(blended_256, cv2.COLOR_GRAY2BGR)

        visual_frame = cv2.resize(pixel_matrix, self.target_resolution, interpolation=cv2.INTER_LINEAR)
        return visual_frame


# ============================================================================
# МОДУЛЬ 5: ДИСКОВЫЙ МЕНЕДЖЕР БИТСТРИМА (.zvd / ZV26)
# ============================================================================
class ZvdBitstreamManager:
    def __init__(self, filename: str):
        self.filename = filename
        self.write_buffer = []

    def write_frame_packet(self, metadata: dict, compressed_residual: bytes):
        va_y, va_x = metadata["vertex_a"]
        vb_y, vb_x = metadata["vertex_b"]
        vc_y, vc_x = metadata["vertex_c"]
        entropy = metadata["entropy"]
        rhythm = metadata["rhythm"]
        
        residual_len = len(compressed_residual)
        
        header = struct.pack("BBBBBBffI", va_y, va_x, vb_y, vb_x, vc_y, vc_x, entropy, rhythm, residual_len)
        self.write_buffer.append(header + compressed_residual)

    def flush_to_disk(self):
        with open(self.filename, "wb") as f:
            f.write(b"ZV26")  # Сигнатура гибридного потока V18-Hybrid
            for packet in self.write_buffer:
                f.write(packet)
        file_size = os.path.getsize(self.filename)
        print(f"[+] Гибридный семантический поток V18-Hybrid успешно записан. Размер файла (.zvd): {file_size} БАЙТ.")
        self.write_buffer.clear()

    def read_from_disk(self) -> list:
        packets_data = []
        header_size = struct.calcsize("BBBBBBffI")
        
        with open(self.filename, "rb") as f:
            magic = f.read(4)
            if magic != b"ZV26":
                raise ValueError("Критическая ошибка: Неверная сигнатура гибридного потока Заводюка!")
                
            while True:
                header_chunk = f.read(header_size)
                if not header_chunk or len(header_chunk) < header_size:
                    break
                    
                va_y, va_x, vb_y, vb_x, vc_y, vc_x, entropy, rhythm, residual_len = struct.unpack("BBBBBBffI", header_chunk)
                compressed_residual = f.read(residual_len)
                
                packets_data.append({
                    "vertex_a": [va_y, va_x],
                    "vertex_b": [vb_y, vb_x],
                    "vertex_c": [vc_y, vc_x],
                    "entropy": entropy,
                    "rhythm": rhythm,
                    "compressed_residual": compressed_residual
                })
        return packets_data


# ============================================================================
# МОДУЛЬ 6: ГЛАВНЫЙ ОРКЕСТРАТОР КОНТУРА И CLI
# ============================================================================
class PentarnyCodecPipeline:
    def __init__(self, grid_size: tuple = (256, 256), display_res: tuple = (640, 480), record_filename: str = "stream_v18_hybrid.zvd"):
        self.grid_size = grid_size
        self.encoder = PentarnyVideoEncoder(grid_size=grid_size)
        self.condenser = PentarnyFieldCondenser(grid_size=grid_size)
        self.residual_engine = ZvdAdaptiveResidualEngine(grid_size=grid_size)
        self.decoder = PentarnyVideoDecoder(grid_size=grid_size, target_resolution=display_res)
        self.bitstream_mgr = ZvdBitstreamManager(record_filename)

    def start_live_stream(self):
        cap = cv2.VideoCapture(0)
        use_synthetic = not cap.isOpened()
        
        if use_synthetic:
            print("[!] Веб-камера недоступна. Запуск синтетического генератора полевых импульсов V18-Hybrid...")
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
                    cv2.rectangle(frame, (x_pos, y_pos), (x_pos + 120, y_pos + 120), (200, 100, 255), -1)
                else:
                    ret, frame = cap.read()
                    if not ret:
                        break

                frame_count += 1

                pentarny_field = self.encoder.process_frame(frame)
                metadata = self.condenser.condense(pentarny_field)
                
                tensor_obj = PentarnyTensorField(self.grid_size)
                evol_obj = PentarnyTensorEvolution(tensor_obj)
                tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_a"]), 2)
                tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_c"]), -2)
                tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_b"]), 0)
                wave_radius = 16 if abs(metadata["rhythm"]) > 0.05 else 8
                evol_obj.propagate_wave(tuple(metadata["vertex_a"]), radius=wave_radius)
                evol_obj.propagate_wave(tuple(metadata["vertex_c"]), radius=wave_radius)
                v18_gray_256 = evol_obj.decode_wave_to_brightness()

                resized_bgr = cv2.resize(frame, self.grid_size, interpolation=cv2.INTER_AREA)
                real_gray_256 = cv2.cvtColor(resized_bgr, cv2.COLOR_BGR2GRAY)

                compressed_residual = self.residual_engine.calculate_and_quantize(
                    real_gray_256, v18_gray_256, metadata["rhythm"]
                )

                self.bitstream_mgr.write_frame_packet(metadata, compressed_residual)

                metadata["compressed_residual"] = compressed_residual
                restored_frame = self.decoder.decode_frame(metadata)

                h, w = restored_frame.shape[:2]
                scale_y, scale_x = h / 256.0, w / 256.0
                
                def draw_vertex(pt, color, label):
                    y_f = int(pt[0] * scale_y)
                    x_f = int(pt[1] * scale_x)
                    cv2.circle(restored_frame, (x_f, y_f), 10, color, -1)
                    cv2.putText(restored_frame, label, (x_f + 14, y_f + 5), 
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

                cv2.imshow("1. ISHODNY POTOK ZAVODJUKA (PRO)", frame)
                cv2.imshow("2. V18-HYBRID FIELD DECODER (256x256)", restored_frame)

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
        finally:
            if not use_synthetic:
                cap.release()
            cv2.destroyAllWindows()
            self.bitstream_mgr.flush_to_disk()
            print("[*] Контур V18-Hybrid успешно завершил сеанс. Физика защищена.")


if __name__ == "__main__":
    print("=" * 70)
    print("🧠 ГИБРИДНЫЙ СЕМАНТИЧЕСКИЙ ВИДЕОКОДЕК ЗАВОДЮКА V18-HYBRID (256x256 / .ZVD)")
    print("   Автор: Владимир Заводюк (Root-Architect) | ALL RIGHTS RESERVED")
    print("=" * 70)

    if len(sys.argv) < 3:
        print("\nИНСТРУКЦИЯ ПО ФИЗИЧЕСКОМУ ЗАПУСКУ ГИБРИДНОГО АРХИВАТОРА:")
        print("-" * 70)
        print("1. ЗАПАКОВКА физического видео-файла (MP4, AVI) в гибридный семантический формат:")
        print("   python zvd_codec_v18_hybrid.py pack path/to/video.mp4 archive_hybrid.zvd")
        print("\n2. РАСПАКОВКА и воспроизведение сжатого гибридного архива:")
        print("   python zvd_codec_v18_hybrid.py unpack archive_hybrid.zvd")
        print("\n3. ЖИВОЙ ТЕСТ с веб-камеры в контуре 256x256 (по умолчанию):")
        print("   python zvd_codec_v18_hybrid.py live")
        print("=" * 70)
        sys.exit(0)

    mode = sys.argv[1].lower()

    if mode == "pack":
        input_video = sys.argv[2]
        output_zvd = sys.argv[3] if len(sys.argv) > 3 else "output_v18_hybrid.zvd"
        
        print(f"[*] Запуск гибридной свертки файла: {input_video} -> {output_zvd}")
        cap = cv2.VideoCapture(input_video)
        if not cap.isOpened():
            print(f"[-] Критическая ошибка: Не удалось открыть видеофайл {input_video}")
            sys.exit(1)
            
        grid_size = (256, 256)
        encoder = PentarnyVideoEncoder(grid_size=grid_size)
        condenser = PentarnyFieldCondenser(grid_size=grid_size)
        residual_engine = ZvdAdaptiveResidualEngine(grid_size=grid_size)
        bitstream_mgr = ZvdBitstreamManager(output_zvd)
        
        frame_idx = 0
        start_time = time.perf_counter()
        
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            
            pentarny_field = encoder.process_frame(frame)
            metadata = condenser.condense(pentarny_field)
            
            tensor_obj = PentarnyTensorField(grid_size)
            evol_obj = PentarnyTensorEvolution(tensor_obj)
            tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_a"]), 2)
            tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_c"]), -2)
            tensor_obj.inject_quantum_impulse(tuple(metadata["vertex_b"]), 0)
            wave_radius = 16 if abs(metadata["rhythm"]) > 0.05 else 8
            evol_obj.propagate_wave(tuple(metadata["vertex_a"]), radius=wave_radius)
            evol_obj.propagate_wave(tuple(metadata["vertex_c"]), radius=wave_radius)
            v18_gray_256 = evol_obj.decode_wave_to_brightness()

            resized_bgr = cv2.resize(frame, grid_size, interpolation=cv2.INTER_AREA)
            real_gray_256 = cv2.cvtColor(resized_bgr, cv2.COLOR_BGR2GRAY)

            compressed_residual = residual_engine.calculate_and_quantize(
                real_gray_256, v18_gray_256, metadata["rhythm"]
            )
            
            bitstream_mgr.write_frame_packet(metadata, compressed_residual)
            
            frame_idx += 1
            if frame_idx % 100 == 0:
                print(f" -> Обработано кадров: {frame_idx}...")
                
        cap.release()
        bitstream_mgr.flush_to_disk()
        total_time = time.perf_counter() - start_time
        print(f"[+] Процесс гибридного сжатия завершен! Время: {total_time:.2f} сек. Кадров: {frame_idx}")

    elif mode == "unpack":
        input_zvd = sys.argv[2]
        print(f"[*] Чтение гибридного семантического потока с диска: {input_zvd}")
        
        bitstream_mgr = ZvdBitstreamManager(input_zvd)
        try:
            packets_data = bitstream_mgr.read_from_disk()
        except Exception as e:
            print(f"[-] Ошибка чтения файла: {e}")
            sys.exit(1)
            
        print(f"[+] Успешно считано {len(packets_data)} кадров. Запуск волнового гибридного декодера V18-Hybrid...")
        decoder = PentarnyVideoDecoder(grid_size=(256, 256), target_resolution=(640, 480))
        
        for metadata in packets_data:
            start_frame_time = time.perf_counter()
            
            restored_frame = decoder.decode_frame(metadata)
            
            cv2.putText(restored_frame, f"Entropy: {metadata['entropy']:.4f}", (20, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            cv2.putText(restored_frame, f"Rhythm: {metadata['rhythm']:+.4f}", (20, 60), 
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
            
            cv2.imshow("PENTARNYI HYBRID ПЛЕЕР V18-HYBRID (256x256)", restored_frame)
            
            elapsed = time.perf_counter() - start_frame_time
            delay = max(1, int((1.0 / 24.0 - elapsed) * 1000))
            if cv2.waitKey(delay) & 0xFF == ord('q'):
                break
                
        cv2.destroyAllWindows()
        print("[*] Воспроизведение гибридного семантического потока завершено.")
        
    elif mode == "live":
        pipeline = PentarnyCodecPipeline()
        pipeline.start_live_stream()
