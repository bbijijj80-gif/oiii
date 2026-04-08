/*
 * TestClient.cpp
 * 
 * Тестовое пользовательское приложение для взаимодействия с драйвером LegacyWdmDriver
 * 
 * Компиляция:
 * cl.exe /EHsc /W4 TestClient.cpp
 * 
 * Запуск:
 * От имени администратора!
 * TestClient.exe
 */

#include <windows.h>
#include <iostream>
#include <iomanip>

// Определение IOCTL кодов (должны совпадать с драйвером)
#define IOCTL_GET_PROCESS_LIST \
    CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TEST_CONNECTION \
    CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Структура для передачи информации о процессе (должна совпадать с драйвером)
typedef struct _PROCESS_INFO {
    ULONG ProcessId;
    ULONG ParentProcessId;
    USHORT NameLength;
    WCHAR ProcessName[64];
} PROCESS_INFO, *PPROCESS_INFO;

// Путь к устройству драйвера
const wchar_t* DEVICE_PATH = L"\\\\.\\LegacyWdmDriver";

void PrintProcessList(PROCESS_INFO* processList, DWORD count) {
    std::wcout << L"\n=== Список активных процессов ===" << std::endl;
    std::wcout << L"----------------------------------------" << std::endl;
    std::wcout << L"№\tPID\tPPID\tИмя процесса" << std::endl;
    std::wcout << L"----------------------------------------" << std::endl;
    
    for (DWORD i = 0; i < count; i++) {
        std::wcout << i + 1 << L"\t"
                   << processList[i].ProcessId << L"\t"
                   << processList[i].ParentProcessId << L"\t"
                   << processList[i].ProcessName << std::endl;
    }
    
    std::wcout << L"----------------------------------------" << std::endl;
    std::wcout << L"Всего процессов: " << count << std::endl;
}

int main() {
    HANDLE hDevice;
    DWORD bytesReturned;
    BOOL result;
    
    std::wcout << L"Тестовое приложение для LegacyWdmDriver" << std::endl;
    std::wcout << L"========================================" << std::endl;
    
    // Открытие устройства драйвера
    hDevice = CreateFileW(
        DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,                              // Нет совместного доступа
        NULL,                           // Атрибуты безопасности по умолчанию
        OPEN_EXISTING,                  // Открыть существующее устройство
        0,                              // Флаги атрибутов
        NULL                            // Нет шаблона файла
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Ошибка открытия устройства: " << GetLastError() << std::endl;
        std::wcerr << L"Убедитесь, что драйвер загружен и запущен" << std::endl;
        std::wcerr << L"Запустите программу от имени администратора!" << std::endl;
        return 1;
    }
    
    std::wcout << L"Устройство успешно открыто" << std::endl;
    
    // Тест 1: Проверка соединения
    std::wcout << L"\n--- Тест 1: Проверка соединения ---" << std::endl;
    
    WCHAR testBuffer[256];
    ZeroMemory(testBuffer, sizeof(testBuffer));
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_TEST_CONNECTION,
        NULL,               // Входной буфер (не требуется)
        0,                  // Размер входного буфера
        testBuffer,         // Выходной буфер
        sizeof(testBuffer), // Размер выходного буфера
        &bytesReturned,     // Количество возвращенных байт
        NULL                // overlapped структура (не требуется)
    );
    
    if (result) {
        std::wcout << L"Ответ драйвера: " << testBuffer << std::endl;
    } else {
        std::wcerr << L"Ошибка IOCTL_TEST_CONNECTION: " << GetLastError() << std::endl;
    }
    
    // Тест 2: Получение списка процессов
    std::wcout << L"\n--- Тест 2: Получение списка процессов ---" << std::endl;
    
    // Выделение буфера для списка процессов
    const DWORD MAX_PROCESSES = 256;
    PROCESS_INFO* processList = new PROCESS_INFO[MAX_PROCESSES];
    ZeroMemory(processList, MAX_PROCESSES * sizeof(PROCESS_INFO));
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_GET_PROCESS_LIST,
        NULL,                           // Входной буфер (не требуется)
        0,                              // Размер входного буфера
        processList,                    // Выходной буфер
        MAX_PROCESSES * sizeof(PROCESS_INFO), // Размер выходного буфера
        &bytesReturned,                 // Количество возвращенных байт
        NULL                            // overlapped структура (не требуется)
    );
    
    if (result) {
        DWORD processCount = bytesReturned / sizeof(PROCESS_INFO);
        PrintProcessList(processList, processCount);
    } else {
        std::wcerr << L"Ошибка IOCTL_GET_PROCESS_LIST: " << GetLastError() << std::endl;
    }
    
    // Освобождение памяти
    delete[] processList;
    
    // Закрытие дескриптора устройства
    CloseHandle(hDevice);
    
    std::wcout << L"\n========================================" << std::endl;
    std::wcout << L"Тестирование завершено" << std::endl;
    
    return 0;
}
