# Legacy WDM Driver для Windows 11

## Обзор

Этот проект содержит пример **Software-Only Legacy WDM драйвера** для Windows 11, реализующего интерфейс IOCTL (Buffered I/O) для связи пользовательского приложения с ядром операционной системы.

## Структура проекта

```
/workspace/
├── LegacyWdmDriver.c    # Исходный код драйвера
├── LegacyWdmDriver.inf  # INF файл для установки драйвера
├── TestClient.cpp       # Тестовое пользовательское приложение
└── README.md           # Эта документация
```

## Возможности драйвера

### Реализованный функционал:

1. **Создание устройства** через `IoCreateDevice`
2. **Создание символьной ссылки** через `IoCreateSymbolicLink` для доступа из пользовательского режима
3. **Обработка IRP_MJ_DEVICE_CONTROL** для получения команд от пользователя
4. **Перечисление активных процессов** с использованием `PsGetNextProcess` и выводом в `DbgPrint`

### IOCTL команды:

| Код | Назначение | Метод I/O |
|-----|------------|-----------|
| `IOCTL_GET_PROCESS_LIST` (0x8000, 0x800) | Получение списка активных процессов | Buffered I/O |
| `IOCTL_TEST_CONNECTION` (0x8000, 0x801) | Проверка соединения с драйвером | Buffered I/O |

## Требования для сборки

### Необходимое ПО:

1. **Visual Studio 2019/2022** с рабочей нагрузкой:
   - Разработка классических приложений на C++
   
2. **Windows Driver Kit (WDK)** последней версии
   - Скачать: https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk
   
3. **Windows SDK**

## Инструкция по сборке в Visual Studio

### Шаг 1: Создание проекта драйвера

1. Откройте Visual Studio
2. **Файл → Создать → Проект**
3. Выберите шаблон: **Kernel Mode Driver (KMDF/WDM)**
4. Укажите имя проекта: `LegacyWdmDriver`
5. Выберите расположение проекта

### Шаг 2: Добавление файлов

1. Добавьте файл `LegacyWdmDriver.c` в проект драйвера
2. Добавьте файл `LegacyWdmDriver.inf` в проект драйвера
3. Удалите автоматически созданные файлы драйвера (если есть)

### Шаг 3: Настройка свойств проекта

Откройте **Свойства проекта** и настройте:

#### Конфигурация драйвера:
- **Configuration Type**: Driver
- **Target Platform Version**: Windows 10 или Windows 11
- **Platform Toolset**: Windows Kernel Mode Drivers

#### C/C++ настройки:
- **Warning Level**: Level 4 (/W4)
- **Treat Warning As Errors**: Yes (/WX) - по умолчанию в WDK
- **SDL checks**: No (для учебных целей)

**Важно**: Если вы получаете ошибки компиляции из-за недокументированных функций, убедитесь, что в файле драйвера добавлены прототипы функций после `#include`:

```c
// Прототипы недокументированных функций ядра
NTKERNELAPI PCHAR NTAPI PsGetProcessImageFileName(_In_ PEPROCESS Process);
NTKERNELAPI NTSTATUS NTAPI PsGetNextProcess(_In_ PEPROCESS Process, _Out_ PEPROCESS *NextProcess);
NTKERNELAPI HANDLE NTAPI PsGetInheritedFromUniqueProcessId(_In_ PEPROCESS Process);
```

#### Linker настройки:
- **Additional Library Directories**: `$(DDK_LIB_PATH)`
- **Additional Dependencies**: 
  ```
  ntoskrnl.lib
  hal.lib
  wdm.lib
  ```

### Шаг 4: Сборка драйвера

1. Выберите конфигурацию: **Debug** или **Release**
2. Выберите платформу: **x64**
3. **Сборка → Сборка решения** (Ctrl+Shift+B)
4. Выходные файлы будут в папке: `x64\Debug\` или `x64\Release\`

## Инструкция по установке драйвера

### Подготовка системы

⚠️ **Важно**: Для установки неподписанного драйвера требуется отключить проверку подписи

1. Запустите командную строку от имени администратора
2. Выполните команду:
   ```cmd
   bcdedit /set testsigning on
   ```
3. Перезагрузите компьютер

### Установка драйвера

#### Способ 1: Через pnputil (рекомендуется)

```cmd
pnputil /add-driver LegacyWdmDriver.inf /install
```

#### Способ 2: Через диспетчер устройств

1. Откройте **Диспетчер устройств**
2. **Действие → Добавить новое устройство**
3. Выберите **Установка оборудования, выбранного вручную**
4. Выберите **Показать все устройства**
5. Нажмите **Установить с диска**
6. Укажите путь к файлу `LegacyWdmDriver.inf`
7. Следуйте инструкциям мастера установки

### Проверка установки

1. Откройте **Диспетчер устройств**
2. Найдите устройство **Legacy WDM Driver Device**
3. Проверьте статус устройства (должен быть "Устройство работает нормально")

## Сборка тестового клиента

### Компиляция

```cmd
cl.exe /EHsc /W4 TestClient.cpp
```

Или через Visual Studio:
1. Создайте новый проект **Console App**
2. Добавьте файл `TestClient.cpp`
3. Соберите проект

## Использование

### Запуск тестового клиента

⚠️ **Требуются права администратора!**

```cmd
TestClient.exe
```

### Ожидаемый вывод

```
Тестовое приложение для LegacyWdmDriver
========================================
Устройство успешно открыто

--- Тест 1: Проверка соединения ---
Ответ драйвера: Driver Connection Test Successful

--- Тест 2: Получение списка процессов ---

=== Список активных процессов ===
----------------------------------------
№	PID	PPID	Имя процесса
----------------------------------------
1	4	0	System
2	100	4	smss.exe
3	120	4	csrss.exe
...
----------------------------------------
Всего процессов: XX

========================================
Тестирование завершено
```

## Просмотр отладочной информации

Для просмотра сообщений `DbgPrint` используйте утилиту **DebugView**:

1. Скачайте DebugView из [Sysinternals](https://learn.microsoft.com/en-us/sysinternals/downloads/debugview)
2. Запустите от имени администратора
3. Включите захват ядерных сообщений (**Capture → Capture Kernel**)
4. Запустите тестовое приложение

## Удаление драйвера

### Остановка службы

```cmd
sc stop LegacyWdmDriver
sc delete LegacyWdmDriver
```

### Возврат проверки подписи

```cmd
bcdedit /set testsigning off
```

Перезагрузите компьютер.

## Безопасность

### ⚠️ Важные предупреждения

1. **Только для учебных целей**: Этот драйвер предназначен исключительно для изучения архитектуры драйверов Windows.

2. **Не используйте в production**: Код не прошел аудит безопасности и не должен использоваться в производственных системах.

3. **Ограничения современных систем**:
   - Windows 11 имеет дополнительные механизмы защиты (HVCI, VBS, PatchGuard)
   - Перечисление процессов из ядра может блокироваться
   - Требуется специальная конфигурация системы для разработки драйверов

4. **Альтернативы для production**:
   - Используйте официальные API пользовательского режима
   - Для мониторинга процессов: Tool Help API, WMI, ETW
   - Для взаимодействия с ядром: рассмотрите использование фильтров или официально поддерживаемых интерфейсов

## Архитектурные особенности

### Buffered I/O

Драйвер использует метод **METHOD_BUFFERED** для IOCTL операций:

- Данные копируются в системный буфер (`SystemBuffer`)
- Автоматическая проверка прав доступа
- Более безопасно, но менее производительно чем Direct I/O

### Обработка IRP

```
IRP_MJ_CREATE      → DispatchCreateClose
IRP_MJ_CLOSE       → DispatchCreateClose  
IRP_MJ_DEVICE_CONTROL → DispatchDeviceControl
```

### Перечисление процессов

Используется функция `PsGetNextProcess` с правильным управлением ссылками:
- `KeEnterCriticalRegion()` для безопасного перебора
- `ObDereferenceObject()` для освобождения ссылок
- `KeLeaveCriticalRegion()` для разблокировки планировщика

## Дополнительные ресурсы

- [Microsoft Docs: Writing a WDM Driver](https://docs.microsoft.com/en-us/windows-hardware/drivers/wdm/)
- [Microsoft Docs: IOCTL Codes](https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/defining-i-o-control-codes)
- [Microsoft Docs: Process Enumeration](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-psgetnextprocess)
- [OSR Online: Driver Development Resources](https://www.osronline.com/)

## Лицензия

Этот код предоставлен исключительно в образовательных целях. Используйте на свой страх и риск.
