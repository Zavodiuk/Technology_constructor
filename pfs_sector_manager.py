# =====================================================================
# MODULE: pfs_sector_manager.py (Pentanary File System Manager)
# =====================================================================

class PFSSector:
    def __init__(self, sector_id, capacity_trits=256):
        self.sector_id = sector_id
        # Физический сектор инициализируется в состоянии идеального равновесия (0)
        self.storage = [0] * capacity_trits
        
    def write_compressed_cluster(self, address, pentanary_data):
        """Записывает сжатый кластер. Нули физически игнорируются для экономии RRAM."""
        for i, trit in enumerate(pentanary_data):
            if address + i < len(self.storage):
                # Запись происходит только при наличии отклонения от баланса
                self.storage[address + i] = trit

    def read_sector_sparsity(self):
        """Возвращает коэффициент фрактальной разреженности сектора."""
        zeros_count = self.storage.count(0)
        return zeros_count / len(self.storage)
