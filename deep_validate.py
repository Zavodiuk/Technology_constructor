import re

def deep_analyze_matrix(filename="protective_property_matrix_full.md"):
    print(f"=== ГЛУБОКИЙ АНАЛИЗ ВСЕХ 60 БЛОКОВ: {filename} ===")
    
    try:
        with open(filename, "r", encoding="utf-8") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Критическая ошибка: файл {filename} не найден.")
        return

    # 1. Анализ корневых блоков кода
    python_blocks = re.findall(r"```python(.*?)```", content, re.DOTALL)
    print(f"\n[1] Обнаружено корневых исполняемых модулей Python: {len(python_blocks)}")
    for idx, code in enumerate(python_blocks, 1):
        try:
            compile(code, f"<root_code_{idx}>", "exec")
            print(f"    -> Корневой модуль #{idx}: синтаксис корректен [OK]")
        except SyntaxError as e:
            print(f"    -> Корневой модуль #{idx}: синтаксическая ошибка: {e} [FAIL]")

    # 2. Поблочный анализ (с 1 по 60)
    print("\n[2] Поэлементная проверка блоков (1–60):")
    blocks = re.split(r"(?=## Блок \d+)", content)
    
    found_blocks = {}
    for block in blocks:
        match = re.search(r"## Блок (\d+)", block)
        if match:
            b_num = int(match.group(1))
            # Проверяем наличие кода или логического описания внутри блока
            has_code_snippet = "```" in block
            found_blocks[b_num] = has_code_snippet

    missing_blocks = []
    blocks_with_snippets = 0
    
    for i in range(1, 61):
        if i in found_blocks:
            status = "содержит код/разметку" if found_blocks[i] else "только текстовое описание"
            if found_blocks[i]:
                blocks_with_snippets += 1
        else:
            missing_blocks.append(i)

    print(f"    -> Всего зарегистрировано блоков в тексте: {len(found_blocks)} из 60")
    print(f"    -> Блоков с детальными программными/структурными вставками: {blocks_with_snippets}")
    
    if missing_blocks:
        print(f"    -> Внимание, пропущены блоки: {missing_blocks}")
    else:
        print("    -> Покрытие структуры: 100% (все 60 блоков на месте) [OK]")

    print("\n=== АНАЛИЗ ЗАВЕРШЕН УСПЕШНО ===")

if __name__ == "__main__":
    deep_analyze_matrix()
