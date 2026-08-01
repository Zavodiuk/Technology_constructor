# =====================================================================
# MODULE: tmtp_packetizer.py (Pentanary Native Packager)
# =====================================================================

def byte_to_pentanary(byte_value):
    """Переводит знаковый байт (-128..127) в 4 сбалансированных пента-трита."""
    val = byte_value
    trits = []
    for _ in range(4):
        remainder = ((val + 2) % 5) - 2
        trits.append(remainder)
        val = (val - remainder) // 5
    return trits

def build_tmtp_packet(binary_payload, header_vector=[1, 2, 0, -1]):
    """Формирует пакет TMTP с автоматическим сжатием вакуумных нулей."""
    raw_pentanary_stream = []
    for byte in binary_payload:
        raw_pentanary_stream.extend(byte_to_pentanary(byte))
        
    sparsity_map = []
    compressed_payload = []
    
    for trit in raw_pentanary_stream:
        if trit == 0:
            sparsity_map.append(0)
        else:
            sparsity_map.append(1)
            compressed_payload.append(trit)
            
    return {
        "header": header_vector,
        "sparsity_map": sparsity_map,
        "payload": compressed_payload
    }
