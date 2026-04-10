/*
 * LegacyWdmDriver.c
 * 
 * Software-Only Legacy WDM Driver для Windows 11
 * Реализует IOCTL интерфейс (Buffered I/O) для связи с пользовательским приложением
 * 
 * Требования для сборки:
 * 1. Установите Windows Driver Kit (WDK) и Visual Studio
 * 2. Создайте новый проект "Kernel Mode Driver" в Visual Studio
 * 3. Добавьте этот файл в проект
 * 4. Настройте INF файл для установки драйвера
 * 
 * Внимание: Этот драйвер предназначен только для учебных целей!
 * Перечисление процессов из ядра требует особых прав и может быть нестабильным
 * в современных версиях Windows из-за защитных механизмов (PatchGuard, HVCI и т.д.)
 */

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

// Прототипы недокументированных функций ядра
// Эти функции отсутствуют в стандартных заголовках WDK, но доступны в ntoskrnl.exe
NTKERNELAPI PCHAR NTAPI PsGetProcessImageFileName(_In_ PEPROCESS Process);
NTKERNELAPI NTSTATUS NTAPI PsGetNextProcess(_In_ PEPROCESS Process, _Out_ PEPROCESS *NextProcess);
NTKERNELAPI HANDLE NTAPI PsGetInheritedFromUniqueProcessId(_In_ PEPROCESS Process);

// Определение устройства и символьной ссылки
#define DEVICE_NAME L"\\Device\\LegacyWdmDriver"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\LegacyWdmDriver"

// Определение IOCTL кодов
// CTL_CODE(DeviceType, Function, Method, Access)
// DeviceType: 0x8000 - 0xFFFF (зарезервировано для разработчиков)
// Function: 0x800 - 0xFFF (пользовательские функции)
// Method: METHOD_BUFFERED для Buffered I/O
// Access: FILE_ANY_ACCESS - доступно для чтения и записи

#define IOCTL_GET_PROCESS_LIST \
    CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TEST_CONNECTION \
    CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Структура для передачи информации о процессе
typedef struct _PROCESS_INFO {
    ULONG ProcessId;
    ULONG ParentProcessId;
    USHORT NameLength;
    WCHAR ProcessName[64];
} PROCESS_INFO, *PPROCESS_INFO;

// Максимальное количество процессов для возврата
#define MAX_PROCESSES 256

// Глобальные переменные
PDEVICE_OBJECT g_DeviceObject = NULL;
UNICODE_STRING g_SymbolicLinkName = {0};

// Прототипы функций
DRIVER_INITIALIZE DriverEntry;
static NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject);
static VOID DeleteDevice(PDRIVER_OBJECT DriverObject);
static VOID UnloadDriver(PDRIVER_OBJECT DriverObject);

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
static NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

static NTSTATUS GetProcessList(PIRP Irp);
static NTSTATUS TestConnection(PIRP Irp);
static VOID EnumerateProcesses(PPROCESS_INFO ProcessInfo, ULONG MaxCount, PULONG ActualCount);

/*
 * Точка входа драйвера
 * Вызывается менеджером конфигурации при загрузке драйвера
 */
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    
    UNREFERENCED_PARAMETER(RegistryPath);
    
    DbgPrint("[LegacyWdmDriver] DriverEntry called\n");
    
    // Установка указателей на функции обработки IRP
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = UnloadDriver;
    
    // Создание устройства
    status = CreateDevice(DriverObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] Failed to create device: 0x%08X\n", status);
        return status;
    }
    
    DbgPrint("[LegacyWdmDriver] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

/*
 * Создание устройства и символьной ссылки
 */
static NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    
    // Инициализация имени устройства
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    
    // Создание устройства
    // Важно: используем правильный размер устройства (0 для устройства без расширений)
    status = IoCreateDevice(
        DriverObject,           // Объект драйвера
        0,                      // Размер расширения устройства
        &deviceName,            // Имя устройства
        FILE_DEVICE_UNKNOWN,    // Тип устройства (пользовательский)
        0,                      // Характеристики устройства
        FALSE,                  // Не эксклюзивное устройство
        &g_DeviceObject         // Возвращаемый объект устройства
    );
    
    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] IoCreateDevice failed: 0x%08X\n", status);
        return status;
    }
    
    // Инициализация имени символьной ссылки
    RtlInitUnicodeString(&g_SymbolicLinkName, SYMBOLIC_LINK_NAME);
    
    // Создание символьной ссылки для доступа из пользовательского режима
    status = IoCreateSymbolicLink(&g_SymbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] IoCreateSymbolicLink failed: 0x%08X\n", status);
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
        return status;
    }
    
    // Установка флагов устройства
    // DO_BUFFERED_IO указывает на использование буферизованного I/O
    g_DeviceObject->Flags |= DO_BUFFERED_IO;
    
    DbgPrint("[LegacyWdmDriver] Device and symbolic link created successfully\n");
    return STATUS_SUCCESS;
}

/*
 * Удаление устройства и символьной ссылки
 */
static VOID DeleteDevice(PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    
    // Удаление символьной ссылки
    if (g_SymbolicLinkName.Buffer != NULL) {
        IoDeleteSymbolicLink(&g_SymbolicLinkName);
    }
    
    // Удаление устройства
    if (g_DeviceObject != NULL) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
    }
    
    DbgPrint("[LegacyWdmDriver] Device and symbolic link deleted\n");
}

/*
 * Функция выгрузки драйвера
 * Вызывается при остановке драйвера
 */
static VOID UnloadDriver(PDRIVER_OBJECT DriverObject)
{
    DbgPrint("[LegacyWdmDriver] UnloadDriver called\n");
    DeleteDevice(DriverObject);
}

/*
 * Обработчик IRP_MJ_CREATE и IRP_MJ_CLOSE
 * Просто возвращает успех для этих операций
 */
static NTSTATUS DispatchCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

/*
 * Обработчик IRP_MJ_DEVICE_CONTROL
 * Обрабатывает IOCTL запросы от пользовательского приложения
 */
static NTSTATUS DispatchDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION stackLocation;
    ULONG controlCode;
    ULONG bytesReturned = 0;
    
    UNREFERENCED_PARAMETER(DeviceObject);
    
    // Получение текущей позиции стека IRP
    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    controlCode = stackLocation->Parameters.DeviceIoControl.IoControlCode;
    
    switch (controlCode) {
        case IOCTL_GET_PROCESS_LIST:
            DbgPrint("[LegacyWdmDriver] Received IOCTL_GET_PROCESS_LIST\n");
            status = GetProcessList(Irp);
            if (NT_SUCCESS(status)) {
                // Получаем количество возвращенных байт из контекста IRP
                bytesReturned = (ULONG)Irp->IoStatus.Information;
            }
            break;
            
        case IOCTL_TEST_CONNECTION:
            DbgPrint("[LegacyWdmDriver] Received IOCTL_TEST_CONNECTION\n");
            status = TestConnection(Irp);
            if (NT_SUCCESS(status)) {
                bytesReturned = (ULONG)Irp->IoStatus.Information;
            }
            break;
            
        default:
            DbgPrint("[LegacyWdmDriver] Unknown IOCTL code: 0x%08X\n", controlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    // Завершение IRP
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return status;
}

/*
 * Обработка IOCTL_GET_PROCESS_LIST
 * Возвращает список активных процессов в буфер пользователя
 */
static NTSTATUS GetProcessList(PIRP Irp)
{
    PIO_STACK_LOCATION stackLocation;
    PPROCESS_INFO processInfo;
    ULONG outputBufferLength;
    ULONG actualCount = 0;
    
    // Получение информации о запросе
    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    outputBufferLength = stackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    
    // Проверка размера буфера
    if (outputBufferLength < sizeof(PROCESS_INFO)) {
        DbgPrint("[LegacyWdmDriver] Output buffer too small\n");
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    // Получение указателя на буфер вывода (для METHOD_BUFFERED это SystemBuffer)
    processInfo = (PPROCESS_INFO)Irp->AssociatedIrp.SystemBuffer;
    
    if (processInfo == NULL) {
        DbgPrint("[LegacyWdmDriver] SystemBuffer is NULL\n");
        return STATUS_INVALID_USER_BUFFER;
    }
    
    // Расчет максимального количества процессов, которое поместится в буфер
    ULONG maxProcesses = outputBufferLength / sizeof(PROCESS_INFO);
    if (maxProcesses > MAX_PROCESSES) {
        maxProcesses = MAX_PROCESSES;
    }
    
    // Перечисление процессов
    EnumerateProcesses(processInfo, maxProcesses, &actualCount);
    
    if (actualCount == 0) {
        DbgPrint("[LegacyWdmDriver] No processes enumerated\n");
        return STATUS_UNSUCCESSFUL;
    }
    
    // Установка количества возвращенных байт
    Irp->IoStatus.Information = actualCount * sizeof(PROCESS_INFO);
    
    DbgPrint("[LegacyWdmDriver] Successfully enumerated %lu processes\n", actualCount);
    
    return STATUS_SUCCESS;
}

/*
 * Обработка IOCTL_TEST_CONNECTION
 * Простой тестовый запрос для проверки соединения
 */
static NTSTATUS TestConnection(PIRP Irp)
{
    PIO_STACK_LOCATION stackLocation;
    PVOID outputBuffer;
    ULONG outputBufferLength;
    const WCHAR* testMessage = L"Driver Connection Test Successful";
    size_t messageLength;
    
    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    outputBufferLength = stackLocation->Parameters.DeviceIoControl.OutputBufferLength;
    outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    
    if (outputBuffer == NULL) {
        return STATUS_INVALID_USER_BUFFER;
    }
    
    messageLength = wcslen(testMessage) + 1; // Включая null-терминатор
    
    if (outputBufferLength < (messageLength * sizeof(WCHAR))) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    // Копирование тестового сообщения в буфер
    RtlCopyMemory(outputBuffer, testMessage, messageLength * sizeof(WCHAR));
    
    Irp->IoStatus.Information = messageLength * sizeof(WCHAR);
    
    DbgPrint("[LegacyWdmDriver] Test connection completed\n");
    
    return STATUS_SUCCESS;
}

/*
 * Перечисление активных процессов
 * Использует PsGetNextProcess для безопасного перебора процессов
 * 
 * ВАЖНО: Эта функция предназначена только для учебных целей!
 * В производственных системах перечисление процессов из ядра может быть
 * ограничено политиками безопасности и требует специальных привилегий.
 */
static VOID EnumerateProcesses(
    PPROCESS_INFO ProcessInfo,
    ULONG MaxCount,
    PULONG ActualCount
)
{
    PEPROCESS currentProcess;
    PEPROCESS nextProcess;
    ULONG count = 0;
    NTSTATUS status;
    
    *ActualCount = 0;
    
    // Получение первого процесса (системный процесс)
    currentProcess = PsInitialSystemProcess;
    
    if (currentProcess == NULL) {
        DbgPrint("[LegacyWdmDriver] PsInitialSystemProcess is NULL\n");
        return;
    }
    
    // Блокировка планировщика для безопасного перебора процессов
    // Это необходимо, так как список процессов может изменяться во время выполнения
    KeEnterCriticalRegion();
    
    do {
        // Проверка на достижение максимального количества процессов
        if (count >= MaxCount) {
            DbgPrint("[LegacyWdmDriver] Reached maximum process count (%lu)\n", MaxCount);
            break;
        }
        
        // Извлечение PID текущего процесса
        HANDLE processId = PsGetProcessId(currentProcess);
        HANDLE parentProcessId = PsGetInheritedFromUniqueProcessId(currentProcess);
        
        // Попытка получить имя процесса
        // Примечание: PsGetProcessImageFileName доступен в новых версиях WDK
        // Для совместимости используем альтернативный метод
        
        PCHAR processNamePtr;
        WCHAR processName[64] = {0};
        
        // Получение имени процесса через PsGetProcessImageFileName (WDK 10.0.14393+)
        #if (_WIN32_WINNT >= 0x0A00)
        processNamePtr = PsGetProcessImageFileName(currentProcess);
        if (processNamePtr != NULL) {
            // Преобразование ANSI в Unicode
            ANSI_STRING ansiName;
            UNICODE_STRING unicodeName;
            
            RtlInitAnsiString(&ansiName, processNamePtr);
            unicodeName.Buffer = processName;
            unicodeName.Length = 0;
            unicodeName.MaximumLength = sizeof(processName) - sizeof(WCHAR);
            
            status = RtlAnsiStringToUnicodeString(&unicodeName, &ansiName, FALSE);
            if (!NT_SUCCESS(status)) {
                // Если преобразование не удалось, копируем как есть
                size_t nameLen = strlen(processNamePtr);
                if (nameLen >= 64) nameLen = 63;
                for (size_t i = 0; i < nameLen; i++) {
                    processName[i] = (WCHAR)processNamePtr[i];
                }
                processName[nameLen] = L'\0';
            }
        } else {
            wcscpy_s(processName, 64, L"Unknown");
        }
        #else
        // Для старых версий WDK
        wcscpy_s(processName, 64, L"LegacyProcess");
        #endif
        
        // Заполнение структуры информации о процессе
        ProcessInfo[count].ProcessId = (ULONG)HandleToULong(processId);
        ProcessInfo[count].ParentProcessId = (ULONG)HandleToULong(parentProcessId);
        ProcessInfo[count].NameLength = (USHORT)wcslen(processName);
        
        RtlCopyMemory(
            ProcessInfo[count].ProcessName,
            processName,
            ((ProcessInfo[count].NameLength * sizeof(WCHAR)) < (sizeof(processName) - sizeof(WCHAR))) ? 
            (ProcessInfo[count].NameLength * sizeof(WCHAR)) : (sizeof(processName) - sizeof(WCHAR))
        );
        
        // Вывод информации в отладочную консоль
        DbgPrint("[LegacyWdmDriver] Process[%lu]: PID=%lu, PPID=%lu, Name=%ws\n",
                 count,
                 ProcessInfo[count].ProcessId,
                 ProcessInfo[count].ParentProcessId,
                 ProcessInfo[count].ProcessName);
        
        count++;
        
        // Переход к следующему процессу
        status = PsGetNextProcess(currentProcess, &nextProcess);
        
        if (!NT_SUCCESS(status)) {
            // Достигнут конец списка процессов
            break;
        }
        
        // Освобождение ссылки на предыдущий процесс
        ObDereferenceObject(currentProcess);
        currentProcess = nextProcess;
        
    } while (currentProcess != NULL);
    
    // Освобождение ссылки на последний процесс, если он есть
    if (currentProcess != NULL) {
        ObDereferenceObject(currentProcess);
    }
    
    // Разблокировка планировщика
    KeLeaveCriticalRegion();
    
    *ActualCount = count;
    
    DbgPrint("[LegacyWdmDriver] Enumeration completed. Total processes: %lu\n", count);
}
