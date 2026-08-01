# =====================================================================
# MODULE: pchain_consensus.py (Proof-of-Tension Blockchain Consensus)
# =====================================================================

class PChainConsensus:
    def __init__(self, node_id):
        self.node_id = node_id
        self.blockchain = []
        
    def calculate_block_tension(self, block_data):
        """Оценивает геометрию блока. Крайние точки [-2, +2] вызывают перегрузку."""
        tension_score = sum(abs(trit) for trit in block_data)
        return tension_score

    def validate_and_append_block(self, new_block):
        """Валидация нового блока через силовой баланс Proof-of-Tension."""
        data = new_block.get("data", [])
        block_tension = self.calculate_block_tension(data)
        
        # Если блок уводит систему в критический перекос, включается Meta-Rashomon
        if (-2 in data and 2 in data) or (block_tension > 50):
            return {"status": "REJECTED", "reason": "Meta-Rashomon Intervention: High Tension"}
            
        self.blockchain.append(new_block)
        return {"status": "ACCEPTED", "current_chain_len": len(self.blockchain)}
