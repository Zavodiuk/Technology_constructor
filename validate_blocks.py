import re

def validate_matrix_file(filename="protective_property_matrix_full.md"):
    print(f"Запуск валидации файла: {filename}...")
    
    try:
        with open(filename, "r", encoding="utf-8") as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Ошибка: файл {filename} не найден в репозитории.")
        return

    # Проверка наличия программного блока Python
    python_blocks = re.findall(r"```python(.*?)```", content, re.DOTALL)
    if python_blocks:
        print("[OK] Исполняемый программный код на Python успешно обнаружен.")
        for i, code in enumerate(python_blocks, 1):
            print(f"   -> Проверка синтаксиса блока кода #{i}...")
            try:
                compile(code, f"<string_{i}>", "exec")
                print(f"   -> Синтаксис блока #{i} корректен.")
            except SyntaxError as e:
                print(f"   -> Ошибка синтаксиса в блоке #{i}: {e}")
    else:
        print("[ПРЕДУПРЕЖДЕНИЕ] Программный код на Python не найден.")

    # Проверка количества блоков в текстовой части
    blocks_found = re.findall(r"## Блок (\d+)", content)
    print(f"[OK] Найдено текстовых описаний блоков: {len(blocks_found)} из 60.")

    if len(blocks_found) == 60 and python_blocks:
        print("\nВалидация успешно завершена: все 60 блоков и программный код на месте и исправны!")
    else:
        print("\nВнимание: структура требует внимания (количество блоков отличается от 60).")

if __name__ == "__main__":
    validate_matrix_file()
