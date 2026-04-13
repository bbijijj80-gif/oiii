/*
 * Драйвер для TP-LINK TL-WN727N v5.20 (RTL8188EUS chipset)
 * Для Windows Server 2019 x64
 * NDIS 6.0 совместимый мини-драйвер
 */

#include <ndis.h>
#include <usb.h>
#include <usbdlib.h>

#define NIC_VENDOR_ID           0x2357
#define NIC_PRODUCT_ID          0x010C
#define NIC_MAX_PACKET_SIZE     1514
#define NIC_HEADER_SIZE         14
#define NIC_MAX_DATA_SIZE       1500
#define NIC_MIN_PACKET_SIZE     64

typedef struct _MP_ADAPTER {
    NDIS_HANDLE MiniportAdapterHandle;
    NDIS_HANDLE RegisterDeviceContext;
    BOOLEAN     bSurpriseRemoved;
    BOOLEAN     bMediaConnected;
    ULONG       ulVendorId;
    ULONG       ulProductId;
    UCHAR       PermanentAddress[NDIS_PHYSADDR_LENGTH];
    UCHAR       CurrentAddress[NDIS_PHYSADDR_LENGTH];
} MP_ADAPTER, *PMP_ADAPTER;

NDIS_STATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS  MiniportCharacteristics;
    NDIS_STATUS  Status;

    NdisZeroMemory(&MiniportCharacteristics, sizeof(MiniportCharacteristics));
    MiniportCharacteristics.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    MiniportCharacteristics.Header.Size = sizeof(MiniportCharacteristics);
    MiniportCharacteristics.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

    MiniportCharacteristics.MajorNdisVersion = NDIS_MINIPORT_MAJOR_VERSION;
    MiniportCharacteristics.MinorNdisVersion = NDIS_MINIPORT_MINOR_VERSION;

    MiniportCharacteristics.InitializeHandler = MPInitialize;
    MiniportCharacteristics.HaltHandler = MPHalt;
    MiniportCharacteristics.UnloadHandler = MPUnload;
    MiniportCharacteristics.PauseHandler = MPPause;
    MiniportCharacteristics.RestartHandler = MPRestart;
    MiniportCharacteristics.OidRequestHandler = MPOidRequest;
    MiniportCharacteristics.SendNetBufferListsHandler = MPSendNetBufferLists;
    MiniportCharacteristics.ReturnNetBufferListsHandler = MPReturnNetBufferLists;
    MiniportCharacteristics.CancelSendHandler = MPCancelSend;
    MiniportCharacteristics.DevicePnPEventNotifyHandler = MPDevicePnPEventNotify;
    MiniportCharacteristics.ShutdownHandler = MPShutdown;

    Status = NdisMRegisterMiniportDriver(
        DriverObject,
        RegistryPath,
        NULL,
        &MiniportCharacteristics,
        NULL
        );

    return Status;
}

VOID
MPUnload(
    IN PDRIVER_OBJECT  DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);
    NdisMDeregisterMiniportDriver();
}

NDIS_STATUS
MPInitialize(
    IN  NDIS_HANDLE             MiniportAdapterHandle,
    IN  NDIS_HANDLE             MiniportDriverContext,
    IN  PNDIS_MINIPORT_INIT_PARAMETERS  InitParameters
    )
{
    PMP_ADAPTER pAdapter;
    NDIS_STATUS Status;
    NDIS_MEDIUM MediumArray[] = { NdisMedium802_3 };

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    UNREFERENCED_PARAMETER(InitParameters);

    pAdapter = (PMP_ADAPTER)NdisAllocateMemoryWithTagPriority(
        MiniportAdapterHandle,
        sizeof(MP_ADAPTER),
        'TLNK',
        NormalPoolPriority
        );

    if (pAdapter == NULL) {
        return NDIS_STATUS_RESOURCES;
    }

    NdisZeroMemory(pAdapter, sizeof(MP_ADAPTER));
    pAdapter->MiniportAdapterHandle = MiniportAdapterHandle;
    pAdapter->bSurpriseRemoved = FALSE;
    pAdapter->bMediaConnected = FALSE;

    // Генерация случайного MAC-адреса (для демонстрации)
    // В реальном драйвере здесь должно быть чтение из EEPROM
    pAdapter->PermanentAddress[0] = 0x00;
    pAdapter->PermanentAddress[1] = 0xE0;
    pAdapter->PermanentAddress[2] = 0x4C;
    pAdapter->PermanentAddress[3] = 0x01;
    pAdapter->PermanentAddress[4] = 0x02;
    pAdapter->PermanentAddress[5] = 0x03;

    NdisMoveMemory(pAdapter->CurrentAddress, pAdapter->PermanentAddress, NDIS_PHYSADDR_LENGTH);

    Status = NdisMSetMiniportAttributes(
        MiniportAdapterHandle,
        NULL  // Упрощено для примера
        );

    if (Status != NDIS_STATUS_SUCCESS) {
        NdisFreeMemory(pAdapter, 0, 0);
        return Status;
    }

    NdisMIndicateStatusEx(
        MiniportAdapterHandle,
        NULL,
        NdisStatusMediaConnect,
        NULL,
        0
        );

    pAdapter->bMediaConnected = TRUE;

    return NDIS_STATUS_SUCCESS;
}

VOID
MPHalt(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN NDIS_HALT_ACTION  HaltAction
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(HaltAction);

    pAdapter->bMediaConnected = FALSE;
    pAdapter->bSurpriseRemoved = TRUE;

    NdisFreeMemory(pAdapter, 0, 0);
}

NDIS_STATUS
MPPause(
    IN NDIS_HANDLE            MiniportAdapterContext,
    IN PNDIS_MINIPORT_PAUSE_PARAMETERS  PauseParameters
    )
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(PauseParameters);
    return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS
MPRestart(
    IN NDIS_HANDLE            MiniportAdapterContext,
    IN PNDIS_MINIPORT_RESTART_PARAMETERS  RestartParameters
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(RestartParameters);

    pAdapter->bSurpriseRemoved = FALSE;
    return NDIS_STATUS_SUCCESS;
}

VOID
MPSendNetBufferLists(
    IN NDIS_HANDLE            MiniportAdapterContext,
    IN PNDIS_NET_BUFFER_LIST  NetBufferLists,
    IN NDIS_PORT_NUMBER       PortNumber,
    IN ULONG                  SendFlags
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;
    PNDIS_NET_BUFFER_LIST CurrentNbl;
    ULONG Count = 0;

    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(SendFlags);

    if (pAdapter->bSurpriseRemoved || !pAdapter->bMediaConnected) {
        CurrentNbl = NetBufferLists;
        while (CurrentNbl != NULL) {
            PNDIS_NET_BUFFER_LIST NextNbl = NET_BUFFER_LIST_NEXT_NBL(CurrentNbl);
            NET_BUFFER_LIST_STATUS(CurrentNbl) = NDIS_STATUS_MEDIA_DISCONNECTED;
            Count++;
            CurrentNbl = NextNbl;
        }
        NdisMSendNetBufferListsComplete(pAdapter->MiniportAdapterHandle, NetBufferLists, 0);
        return;
    }

    // В реальном драйвере здесь была бы отправка данных через USB
    CurrentNbl = NetBufferLists;
    while (CurrentNbl != NULL) {
        PNDIS_NET_BUFFER_LIST NextNbl = NET_BUFFER_LIST_NEXT_NBL(CurrentNbl);
        NET_BUFFER_LIST_STATUS(CurrentNbl) = NDIS_STATUS_SUCCESS;
        Count++;
        CurrentNbl = NextNbl;
    }

    NdisMSendNetBufferListsComplete(pAdapter->MiniportAdapterHandle, NetBufferLists, 0);
}

VOID
MPReturnNetBufferLists(
    IN NDIS_HANDLE            MiniportAdapterContext,
    IN PNDIS_NET_BUFFER_LIST  NetBufferLists,
    IN ULONG                  ReturnFlags
    )
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetBufferLists);
    UNREFERENCED_PARAMETER(ReturnFlags);
}

VOID
MPCancelSend(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN PVOID        CancelId
    )
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(CancelId);
}

NDIS_STATUS
MPOidRequest(
    IN NDIS_HANDLE          MiniportAdapterContext,
    IN PNDIS_OID_REQUEST    OidRequest
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    switch (OidRequest->DATA.QUERY_INFORMATION.Oid) {
        case OID_GEN_HARDWARE_STATUS:
            *(PNDIS_HARDWARE_STATUS)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = 
                NdisHardwareStatusReady;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(NDIS_HARDWARE_STATUS);
            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
            *(PNDIS_MEDIUM)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = NdisMedium802_3;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_PHYSICAL_MEDIUM:
            *(PNDIS_PHYSICAL_MEDIUM)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = 
                NdisPhysicalMediumWirelessLan;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(NDIS_PHYSICAL_MEDIUM);
            break;

        case OID_GEN_MAC_OPTIONS:
            *(PULONG)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = 
                NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                NDIS_MAC_OPTION_NO_LOOPBACK_FILTER |
                NDIS_MAC_OPTION_FULL_DUPLEX;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG);
            break;

        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
            NdisMoveMemory(
                OidRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                pAdapter->CurrentAddress,
                NDIS_PHYSADDR_LENGTH
                );
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = NDIS_PHYSADDR_LENGTH;
            break;

        case OID_GEN_MAXIMUM_TOTAL_FRAME_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            *(PULONG)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = NIC_MAX_PACKET_SIZE;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            {
                PUCHAR Desc = (PUCHAR)"TP-LINK TL-WN727N v5.20";
                NdisMoveMemory(
                    OidRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                    Desc,
                    21
                    );
                OidRequest->DATA.QUERY_INFORMATION.BytesWritten = 21;
            }
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            *(PULONG)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer = 0x00010001;
            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(ULONG);
            break;

        case OID_GEN_LINK_STATE:
            {
                NDIS_LINK_STATE LinkState;
                NdisZeroMemory(&LinkState, sizeof(LinkState));
                LinkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
                LinkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
                LinkState.Header.Size = sizeof(LinkState);
                LinkState.MediaConnectState = MediaConnectStateConnected;
                LinkState.MediaDuplexState = MediaDuplexStateFull;
                LinkState.XmitLinkSpeed = 150000000;  // 150 Mbps
                LinkState.RcvLinkSpeed = 150000000;
                
                NdisMoveMemory(
                    OidRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                    &LinkState,
                    sizeof(LinkState)
                    );
                OidRequest->DATA.QUERY_INFORMATION.BytesWritten = sizeof(LinkState);
            }
            break;

        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    OidRequest->STATUS = Status;
    NdisMOidRequestComplete(pAdapter->MiniportAdapterHandle, OidRequest);

    return Status;
}

VOID
MPDevicePnPEventNotify(
    IN NDIS_HANDLE             MiniportAdapterContext,
    IN PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;

    switch (NetDevicePnPEvent->DevicePnPEvent) {
        case NdisDevicePnPEventSurpriseRemoved:
            pAdapter->bSurpriseRemoved = TRUE;
            pAdapter->bMediaConnected = FALSE;
            break;

        case NdisDevicePnPEventPowerProfileChanged:
        default:
            break;
    }
}

VOID
MPShutdown(
    IN NDIS_HANDLE  MiniportAdapterContext
    )
{
    PMP_ADAPTER pAdapter = (PMP_ADAPTER)MiniportAdapterContext;
    pAdapter->bMediaConnected = FALSE;
}
