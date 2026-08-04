#include "ustd_memory_manager.hpp" // Путь к вашему заголовочному файлу
#include <cassert>
#include <iostream>

void test_allocation_success() {
    ustd::MemoryManager manager(1024);
    
    // Тестируем выделение памяти под массив из 5 целых чисел
    int* array = manager.allocate<int>(5);
    assert(array != nullptr); // Если память не выделилась, тест упадет здесь
    
    // Проверяем, что память доступна для записи
    for (int i = 0; i < 5; ++i) {
        array[i] = i * 10;
    }
    
    assert(array[2] == 20);
    
    // Освобождаем память
    manager.deallocate(array);
    std::cout << "[УСПЕХ] Тест выделения памяти пройден.\n";
}

void test_zero_allocation() {
    ustd::MemoryManager manager(1024);
    
    // Запрос 0 элементов должен безопасно вернуть nullptr
    int* array = manager.allocate<int>(0);
    assert(array == nullptr);
    
    std::cout << "[УСПЕХ] Тест нулевого выделения пройден.\n";
}

int main() {
    std::cout << "Запуск тестов библиотеки ustd...\n";
    
    test_allocation_success();
    test_zero_allocation();
    
    std::cout << "Все тесты успешно завершены!\n";
    return 0;
}
