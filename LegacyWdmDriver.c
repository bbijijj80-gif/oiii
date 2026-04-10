/*
 * LegacyWdmDriver.c
 *
 * Software-Only Legacy WDM Driver для Windows 11
 * Реализует IOCTL интерфейс (Buffered I/O) для связи с пользовательским приложением
 *
 * Требования для сборки:
 * 1. Установите Windows Driver Kit (WDK) и Visual Studio
 * 2. Создайте новый проект "Kernel Mode Driver, Empty" в Visual Studio
 * 3. Добавьте этот файл в проект
 * 4. Настройте INF файл для установки драйвера
 *
 * Внимание: Этот драйвер предназначен только для учебных целей!
 */

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

// Вспомогательная структура для ZwQuerySystemInformation
typedef struct _SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION, *PSYSTEM_PROCESS_INFORMATION;

// Импорт функции ZwQuerySystemInformation
NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    _In_ ULONG SystemInformationClass,
    _Out_ PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength
);

#define SystemProcessInformation 5

// Определение устройства и символьной ссылки
#define DEVICE_NAME L"\\Device\\LegacyWdmDriver"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\LegacyWdmDriver"

// IOCTL коды
#define IOCTL_GET_PROCESS_LIST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_TEST_CONNECTION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Структура для передачи информации о процессе
typedef struct _PROCESS_INFO {
    ULONG ProcessId;
    ULONG ParentProcessId;
    CHAR ImageName[16];
} PROCESS_INFO, *PPROCESS_INFO;

// Глобальные переменные
PDEVICE_OBJECT g_DeviceObject = NULL;

// Прототипы функций
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
VOID EnumerateProcesses(PPROCESS_INFO ProcessInfo, ULONG MaxCount, PULONG ActualCount);

// Точка входа драйвера
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLinkName;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[LegacyWdmDriver] DriverEntry called\n");

    // Инициализация имени устройства
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);

    // Создание устройства
    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    g_DeviceObject = deviceObject;

    // Создание символьной ссылки
    RtlInitUnicodeString(&symbolicLinkName, SYMBOLIC_LINK_NAME);

    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] IoCreateSymbolicLink failed: 0x%X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    // Установка обработчиков IRP
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[LegacyWdmDriver] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

// Обработчик IRP_MJ_CREATE и IRP_MJ_CLOSE
NTSTATUS DispatchCreateClose(
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

// Обработчик IRP_MJ_DEVICE_CONTROL
NTSTATUS DispatchDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    PIO_STACK_LOCATION irpStack;
    ULONG ioctlCode;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    ioctlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    switch (ioctlCode) {
        case IOCTL_GET_PROCESS_LIST: {
            PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
            PVOID outputBuffer = Irp->AssociatedIrp.SystemBuffer;
            ULONG inputBufferSize = irpStack->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outputBufferSize = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

            // Проверка размера буфера
            if (outputBufferSize < sizeof(PROCESS_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            // Получение максимального количества процессов
            ULONG maxProcesses = 1;
            if (inputBufferSize >= sizeof(ULONG)) {
                maxProcesses = *(PULONG)inputBuffer;
                if (maxProcesses == 0 || maxProcesses > 1024) {
                    maxProcesses = 64; // Значение по умолчанию
                }
            }

            // Вычисление доступного количества структур
            ULONG availableSlots = (outputBufferSize - sizeof(ULONG)) / sizeof(PROCESS_INFO);
            if (maxProcesses > availableSlots) {
                maxProcesses = availableSlots;
            }

            if (maxProcesses == 0) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            // Перечисление процессов
            PPROCESS_INFO processInfo = (PPROCESS_INFO)((PUCHAR)outputBuffer + sizeof(ULONG));
            ULONG actualCount = 0;

            EnumerateProcesses(processInfo, maxProcesses, &actualCount);

            // Запись количества процессов в начало буфера
            *(PULONG)outputBuffer = actualCount;

            bytesReturned = sizeof(ULONG) + (actualCount * sizeof(PROCESS_INFO));
            break;
        }

        case IOCTL_TEST_CONNECTION: {
            DbgPrint("[LegacyWdmDriver] IOCTL_TEST_CONNECTION received\n");
            bytesReturned = 0;
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = (ULONG_PTR)bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

// Функция перечисления процессов через ZwQuerySystemInformation
VOID EnumerateProcesses(
    PPROCESS_INFO ProcessInfo,
    ULONG MaxCount,
    PULONG ActualCount
)
{
    PVOID buffer = NULL;
    ULONG bufferSize = 0;
    NTSTATUS status;

    *ActualCount = 0;

    // Первый вызов для определения необходимого размера буфера
    status = ZwQuerySystemInformation(
        SystemProcessInformation,
        NULL,
        0,
        &bufferSize
    );

    // Ожидаем STATUS_INFO_LENGTH_MISMATCH
    if (status != STATUS_INFO_LENGTH_MISMATCH && !NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] ZwQuerySystemInformation (size query) failed: 0x%X\n", status);
        return;
    }

    // Выделение пула для информации о процессах
    buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, 'Proc');
    if (buffer == NULL) {
        DbgPrint("[LegacyWdmDriver] ExAllocatePool2 failed\n");
        return;
    }

    // Получение информации о процессах
    status = ZwQuerySystemInformation(
        SystemProcessInformation,
        buffer,
        bufferSize,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[LegacyWdmDriver] ZwQuerySystemInformation (data query) failed: 0x%X\n", status);
        ExFreePoolWithTag(buffer, 'Proc');
        return;
    }

    // Обработка списка процессов
    PSYSTEM_PROCESS_INFORMATION currentEntry = (PSYSTEM_PROCESS_INFORMATION)buffer;
    ULONG count = 0;

    while (TRUE) {
        if (count >= MaxCount) {
            DbgPrint("[LegacyWdmDriver] Reached maximum process count (%lu)\n", MaxCount);
            break;
        }

        // Сохранение информации о процессе
        ProcessInfo[count].ProcessId = (ULONG)HandleToULong(currentEntry->UniqueProcessId);
        ProcessInfo[count].ParentProcessId = (ULONG)HandleToULong(currentEntry->InheritedFromUniqueProcessId);

        // Копирование имени процесса
        if (currentEntry->ImageName.Buffer != NULL && currentEntry->ImageName.Length > 0) {
            UNICODE_STRING imageName = currentEntry->ImageName;
            USHORT copyLength = (imageName.Length < sizeof(ProcessInfo[count].ImageName) - 1) 
                                ? imageName.Length 
                                : (USHORT)(sizeof(ProcessInfo[count].ImageName) - 1);
            
            RtlCopyMemory(ProcessInfo[count].ImageName, imageName.Buffer, copyLength);
            ProcessInfo[count].ImageName[copyLength] = '\0';
        } else {
            RtlCopyMemory(ProcessInfo[count].ImageName, "Unknown", 7);
            ProcessInfo[count].ImageName[7] = '\0';
        }

        DbgPrint("[LegacyWdmDriver] PID: %lu, PPID: %lu, Name: %s\n",
                 ProcessInfo[count].ProcessId,
                 ProcessInfo[count].ParentProcessId,
                 ProcessInfo[count].ImageName);

        count++;

        // Переход к следующему элементу
        if (currentEntry->NextEntryOffset == 0) {
            break;
        }

        currentEntry = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)currentEntry + currentEntry->NextEntryOffset);
    }

    *ActualCount = count;

    // Освобождение памяти
    ExFreePoolWithTag(buffer, 'Proc');
}

// Процедура выгрузки драйвера
VOID DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symbolicLinkName;

    DbgPrint("[LegacyWdmDriver] DriverUnload called\n");

    // Удаление символьной ссылки
    RtlInitUnicodeString(&symbolicLinkName, SYMBOLIC_LINK_NAME);
    IoDeleteSymbolicLink(&symbolicLinkName);

    // Удаление устройства
    if (g_DeviceObject != NULL) {
        IoDeleteDevice(g_DeviceObject);
    }

    DbgPrint("[LegacyWdmDriver] Driver unloaded\n");
}
