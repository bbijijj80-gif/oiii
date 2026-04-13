// rtl8188eu.c - NDIS 6.0 Miniport Driver for TP-LINK TL-WN727N v5.20 (RTL8188EU)
// Target: Windows Server 2019 x64

#include <ndis.h>

#define NIC_VENDOR_ID               0x00E04C
#define NIC_MAX_PACKET_SIZE         1514
#define NIC_BUFFER_LENGTH           0x4000
#define NIC_VENDOR_DRIVER_VERSION   0x00010000
#define NIC_NDIS_MINIPORT_MAJOR_VERSION 6
#define NIC_NDIS_MINIPORT_MINOR_VERSION 0

#define MP_ADAPTER_FLAGS_RESET_IN_PROGRESS  0x00000001
#define MP_ADAPTER_FLAGS_SHUTTING_DOWN      0x00000002
#define MP_ADAPTER_FLAGS_SURPRISE_REMOVED   0x00000004
#define MP_ADAPTER_FLAGS_MEDIA_DISCONNECTED 0x00000008

typedef struct _MP_ADAPTER
{
    NDIS_HANDLE MiniportAdapterHandle;
    NDIS_HANDLE MiniportInterruptContext;
    ULONG MtuSize;
    ULONG MediaConnectState;
    ULONG LinkSpeed;
    UCHAR PermanentAddress[NDIS_PHYSADDR_LENGTH];
    UCHAR CurrentAddress[NDIS_PHYSADDR_LENGTH];
    ULONG AdapterFlags;
    ULONG LookaheadSize;
    ULONG MaxTotalFrameSize;
    ULONG PacketFilter;
    ULONG MacOptions;
} MP_ADAPTER, *PMP_ADAPTER;

static NDIS_HANDLE gDriverHandle = NULL;

static NDIS_STATUS
MPInitialize(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportDriverContext,
    IN PNDIS_MINIPORT_INIT_PARAMETERS InitParameters
)
{
    PMP_ADAPTER adapter = NULL;
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES regAttrs;
    NDIS_PM_CAPABILITIES pmCapabilities;

    do
    {
        adapter = (PMP_ADAPTER)NdisAllocateMemoryWithTagPriority(
            MiniportAdapterHandle,
            sizeof(MP_ADAPTER),
            'tuaR',
            NormalPoolPriority
        );

        if (adapter == NULL)
        {
            status = NDIS_STATUS_RESOURCES;
            break;
        }

        NdisZeroMemory(adapter, sizeof(MP_ADAPTER));
        adapter->MiniportAdapterHandle = MiniportAdapterHandle;
        adapter->MediaConnectState = MediaConnectStateDisconnected;
        adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
        adapter->MtuSize = 1500;
        adapter->MaxTotalFrameSize = NIC_MAX_PACKET_SIZE;
        adapter->PacketFilter = NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_DIRECTED;
        adapter->MacOptions = NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                              NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                              NDIS_MAC_OPTION_FULL_DUPLEX |
                              NDIS_MAC_OPTION_NO_LOOPBACK_FILTER;

        // Set permanent MAC address (example)
        adapter->PermanentAddress[0] = 0x00;
        adapter->PermanentAddress[1] = 0xE0;
        adapter->PermanentAddress[2] = 0x4C;
        adapter->PermanentAddress[3] = 0x12;
        adapter->PermanentAddress[4] = 0x34;
        adapter->PermanentAddress[5] = 0x56;
        NdisMoveMemory(adapter->CurrentAddress, adapter->PermanentAddress, NDIS_PHYSADDR_LENGTH);

        NdisZeroMemory(&regAttrs, sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES));
        regAttrs.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
        regAttrs.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);
        regAttrs.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        regAttrs.MiniportAdapterContext = (NDIS_HANDLE)adapter;
        regAttrs.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM | NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK;
        regAttrs.InterfaceType = NDIS_INTERFACE_TYPE_PCI;

        status = NdisMSetMiniportAttributes(MiniportAdapterHandle, &regAttrs);
        if (status != NDIS_STATUS_SUCCESS)
            break;

        // Indicate media connect status
        NdisMIndicateStatusEx(MiniportAdapterHandle, NULL, NdisStatusMediaConnect, NULL, 0);
        adapter->MediaConnectState = MediaConnectStateConnected;
        adapter->LinkSpeed = 150000000; // 150 Mbps

    } while (FALSE);

    if (status != NDIS_STATUS_SUCCESS)
    {
        if (adapter != NULL)
        {
            NdisFreeMemory(adapter, 0, 0);
        }
    }

    return status;
}

static VOID
MPHalt(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_HALT_ACTION HaltAction
)
{
    PMP_ADAPTER adapter = (PMP_ADAPTER)MiniportAdapterContext;

    if (adapter != NULL)
    {
        adapter->MediaConnectState = MediaConnectStateDisconnected;
        NdisFreeMemory(adapter, 0, 0);
    }
}

static VOID
MPUnload(
    IN NDIS_HANDLE MiniportDriverContext
)
{
    UNREFERENCED_PARAMETER(MiniportDriverContext);
}

static NDIS_STATUS
MPPause(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(PauseParameters);
    return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS
MPRestart(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters
)
{
    PMP_ADAPTER adapter = (PMP_ADAPTER)MiniportAdapterContext;
    UNREFERENCED_PARAMETER(RestartParameters);

    adapter->MediaConnectState = MediaConnectStateConnected;
    NdisMIndicateStatusEx(adapter->MiniportAdapterHandle, NULL, NdisStatusMediaConnect, NULL, 0);
    
    return NDIS_STATUS_SUCCESS;
}

static VOID
MPSendNetBufferLists(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_NET_BUFFER_LIST NetBufferLists,
    IN NDIS_PORT_NUMBER PortNumber,
    IN ULONG SendFlags
)
{
    PMP_ADAPTER adapter = (PMP_ADAPTER)MiniportAdapterContext;
    PNDIS_NET_BUFFER_LIST currentNbl = NetBufferLists;
    NDIS_STATUS sendStatus = NDIS_STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(SendFlags);

    while (currentNbl != NULL)
    {
        PNDIS_NET_BUFFER_LIST nextNbl = NET_BUFFER_LIST_NEXT_NBL(currentNbl);
        
        if (adapter->MediaConnectState != MediaConnectStateConnected)
        {
            sendStatus = NDIS_STATUS_MEDIA_DISCONNECTED;
        }
        else
        {
            // In a real driver, we would send the data to hardware here
            sendStatus = NDIS_STATUS_SUCCESS;
        }

        NET_BUFFER_LIST_STATUS(currentNbl) = sendStatus;
        currentNbl = nextNbl;
    }

    NdisMSendNetBufferListsComplete(adapter->MiniportAdapterHandle, NetBufferLists, 0);
}

static VOID
MPReturnNetBufferLists(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_NET_BUFFER_LIST NetBufferLists,
    IN ULONG ReturnFlags
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetBufferLists);
    UNREFERENCED_PARAMETER(ReturnFlags);
}

static VOID
MPCancelSend(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PVOID CancelId
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(CancelId);
}

static VOID
MPDevicePnPEventNotify(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNET_DEVICE_PNP_EVENT NetDevicePnPEvent
)
{
    UNREFERENCED_PARAMETER(MiniportAdapterContext);
    UNREFERENCED_PARAMETER(NetDevicePnPEvent);
}

static NDIS_STATUS
MPOidRequest(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PNDIS_OID_REQUEST OidRequest
)
{
    PMP_ADAPTER adapter = (PMP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    PNDIS_OID_REQUEST_PARAMETERS parms = &OidRequest->DATA.OidRequestParameters;

    switch (parms->Oid)
    {
        case OID_GEN_HARDWARE_STATUS:
            *(PNDIS_HARDWARE_STATUS)parms->Information.Buffer = NdisHardwareStatusReady;
            parms->BytesWritten = sizeof(NDIS_HARDWARE_STATUS);
            parms->BytesNeeded = sizeof(NDIS_HARDWARE_STATUS);
            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
            *(PNDIS_MEDIUM)parms->Information.Buffer = NdisMedium802_3;
            parms->BytesWritten = sizeof(NDIS_MEDIUM);
            parms->BytesNeeded = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
        {
            const char* vendorDesc = "TP-LINK TL-WN727N v5.20";
            size_t copyLen = strlen(vendorDesc) + 1;
            if (copyLen > parms->InformationBufferLength)
                copyLen = parms->InformationBufferLength;
            NdisMoveMemory(parms->Information.Buffer, vendorDesc, copyLen);
            parms->BytesWritten = (ULONG)copyLen;
            parms->BytesNeeded = (ULONG)(strlen(vendorDesc) + 1);
            break;
        }

        case OID_GEN_VENDOR_DRIVER_VERSION:
            *(PULONG)parms->Information.Buffer = NIC_VENDOR_DRIVER_VERSION;
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            break;

        case OID_GEN_VENDOR_ID:
            *(PULONG)parms->Information.Buffer = NIC_VENDOR_ID;
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            *(PULONG)parms->Information.Buffer = adapter->PacketFilter;
            break;

        case OID_GEN_MAC_OPTIONS:
            *(PULONG)parms->Information.Buffer = adapter->MacOptions;
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            break;

        case OID_GEN_MAXIMUM_TOTAL_FRAME_SIZE:
            *(PULONG)parms->Information.Buffer = adapter->MaxTotalFrameSize;
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            *(PULONG)parms->Information.Buffer = adapter->LookaheadSize;
            parms->BytesWritten = sizeof(ULONG);
            parms->BytesNeeded = sizeof(ULONG);
            break;

        case OID_GEN_DRIVER_VERSION:
        {
            USHORT version = (NIC_NDIS_MINIPORT_MAJOR_VERSION << 8) | NIC_NDIS_MINIPORT_MINOR_VERSION;
            *(PUSHORT)parms->Information.Buffer = version;
            parms->BytesWritten = sizeof(USHORT);
            parms->BytesNeeded = sizeof(USHORT);
            break;
        }

        case OID_GEN_PHYSICAL_MEDIUM:
            *(PNDIS_PHYSICAL_MEDIUM)parms->Information.Buffer = NdisPhysicalMediumWirelessLan;
            parms->BytesWritten = sizeof(NDIS_PHYSICAL_MEDIUM);
            parms->BytesNeeded = sizeof(NDIS_PHYSICAL_MEDIUM);
            break;

        case OID_GEN_MEDIA_CONNECT_STATUS:
            *(PNDIS_MEDIA_CONNECT_STATE)parms->Information.Buffer = (NDIS_MEDIA_CONNECT_STATE)adapter->MediaConnectState;
            parms->BytesWritten = sizeof(NDIS_MEDIA_CONNECT_STATE);
            parms->BytesNeeded = sizeof(NDIS_MEDIA_CONNECT_STATE);
            break;

        case OID_GEN_LINK_SPEED:
            *(PULONG64)parms->Information.Buffer = adapter->LinkSpeed;
            parms->BytesWritten = sizeof(ULONG64);
            parms->BytesNeeded = sizeof(ULONG64);
            break;

        case OID_802_3_PERMANENT_ADDRESS:
        case OID_802_3_CURRENT_ADDRESS:
            NdisMoveMemory(parms->Information.Buffer, adapter->PermanentAddress, NDIS_PHYSADDR_LENGTH);
            parms->BytesWritten = NDIS_PHYSADDR_LENGTH;
            parms->BytesNeeded = NDIS_PHYSADDR_LENGTH;
            break;

        default:
            status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    OidRequest->STATUS = status;
    NdisMOidRequestComplete(adapter->MiniportAdapterHandle, OidRequest);

    return status;
}

static NDIS_MINIPORT_DRIVER_CHARACTERISTICS gMiniportChars;

NDIS_STATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
)
{
    NDIS_STATUS status;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS miniportChars;

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NdisZeroMemory(&miniportChars, sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS));

    miniportChars.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
    miniportChars.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    miniportChars.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_6_0;

    miniportChars.MajorNdisVersion = NIC_NDIS_MINIPORT_MAJOR_VERSION;
    miniportChars.MinorNdisVersion = NIC_NDIS_MINIPORT_MINOR_VERSION;

    miniportChars.InitializeHandlerEx = MPInitialize;
    miniportChars.HaltHandlerEx = MPHalt;
    miniportChars.UnloadHandler = MPUnload;
    miniportChars.PauseHandler = MPPause;
    miniportChars.RestartHandler = MPRestart;
    miniportChars.OidRequestHandler = MPOidRequest;
    miniportChars.SendNetBufferListsHandler = MPSendNetBufferLists;
    miniportChars.ReturnNetBufferListsHandler = MPReturnNetBufferLists;
    miniportChars.CancelSendHandler = MPCancelSend;
    miniportChars.DevicePnPEventNotifyHandler = MPDevicePnPEventNotify;

    status = NdisMRegisterMiniportDriver(
        DriverObject,
        RegistryPath,
        (NDIS_HANDLE)NULL,
        &miniportChars,
        &gDriverHandle
    );

    if (status != NDIS_STATUS_SUCCESS)
    {
        return status;
    }

    return NDIS_STATUS_SUCCESS;
}
