```python
import re

def deep_analyze_matrix(filename="protective_property_matrix_full.md"):
    print(f"=== DEEP ANALYSIS OF ALL 60 BLOCKS: {filename} ===")
    
    try:
        with open(filename, "r", encoding="utf-8") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Critical Error: File {filename} not found.")
        return

    # 1. Analysis of root Python code blocks
    python_blocks = re.findall(r"```python(.*?)```", content, re.DOTALL)
    print(f"\n[1] Root executable Python modules detected: {len(python_blocks)}")
    for idx, code in enumerate(python_blocks, 1):
        try:
            compile(code, f"<root_code_{idx}>", "exec")
            print(f"    -> Root module #{idx}: syntax valid [OK]")
        except SyntaxError as e:
            print(f"    -> Root module #{idx}: syntax error: {e} [FAIL]")

    # 2. Block-by-block analysis (1 to 60)
    print("\n[2] Element-by-element block check (1–60):")
    blocks = re.split(r"(?=## Блок \d+)", content)
    
    found_blocks = {}
    for block in blocks:
        match = re.search(r"## Блок (\d+)", block)
        if match:
            b_num = int(match.group(1))
            has_code_snippet = "```" in block
            found_blocks[b_num] = has_code_snippet

    missing_blocks = []
    blocks_with_snippets = 0
    
    for i in range(1, 61):
        if i in found_blocks:
            if found_blocks[i]:
                blocks_with_snippets += 1
        else:
            missing_blocks.append(i)

    print(f"    -> Total registered blocks in text: {len(found_blocks)} out of 60")
    print(f"    -> Blocks with detailed structural/code snippets: {blocks_with_snippets}")
    
    if missing_blocks:
        print(f"    -> Warning, missing blocks: {missing_blocks}")
    else:
        print("    -> Structure coverage: 100% (all 60 blocks present) [OK]")

    print("\n=== ANALYSIS COMPLETED SUCCESSFULLY ===")

if __name__ == "__main__":
    deep_analyze_matrix()
