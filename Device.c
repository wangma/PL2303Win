/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, USBKmDriverCreateDevice)
#pragma alloc_text (PAGE, USBKmDriverEvtDevicePrepareHardware)
#pragma alloc_text (PAGE, USBKmDriverEvtDeviceRelease)
#endif


NTSTATUS
USBKmDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
�������������ù������������������豸���������Դ��

������
DeviceInit - ָ��͸����ʼ���ṹ��ָ�롣�� WdfDeviceCreate �ɹ�ʱ����ܽ��ͷŴ˽ṹ���ڴ档��ˣ��˺�Ҫ���ʸýṹ��

Return Value:
    NTSTATUS
--*/
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_DEVICE_PNP_CAPABILITIES  pnpCaps;
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;

    PAGED_CODE();

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = USBKmDriverEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = USBKmDriverEvtDeviceRelease;

    pnpPowerCallbacks.EvtDeviceD0Entry = PL2303EvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = PL2303EvtDeviceD0Exit;
    pnpPowerCallbacks.EvtDeviceSelfManagedIoFlush = PL2303EvtDeviceSelfManagedIoFlush;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) 
    {
        WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
        pnpCaps.SurpriseRemovalOK = WdfTrue;

        WdfDeviceSetPnpCapabilities(device, &pnpCaps);

        // ��ȡָ�����Ǹո����豸����������豸�����Ľṹ��ָ�롣������ device.h
        // ͷ�ļ��ж���˽ṹ��DeviceGetContext ��ͨ��ʹ�� device.h �е� WDF_DECLARE_CONTEXT_TYPE_WITH_NAME �����ɵ�����������
        // �˺�����ִ�����ͼ�鲢�����豸�����ġ�����������˴���Ķ��������������� NULL �������Ƿ��ڿ����֤��ģʽ�����С�
        deviceContext = DeviceGetContext(device);

        // ��ʼ���豸��չ
        deviceContext->PrivateDeviceData = 0;
        deviceContext->Device = device;

        status = CreateSymbolName(device);
        if (NT_SUCCESS(status)) 
        {
            // ��ʼ�� I/O ���κζ���
            status = USBKmDriverQueueInitialize(device);
        }
    }
    return status;
}

NTSTATUS
USBKmDriverEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
    )
/*++
�����������ڴ˻ص��У���������ִ��һ�б�Ҫ������ʹӲ��׼������������ USB �豸�����漰��ȡ��ѡ����������

Arguments:
    Device - handle to a device

Return Value:
    NT status value
--*/
{
    NTSTATUS status;
    PDEVICE_CONTEXT pDeviceContext;
    WDF_USB_DEVICE_CREATE_CONFIG createParams;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;

    WDF_USB_DEVICE_INFORMATION          deviceInfo;
    WDFUSBPIPE                          pipe;
    WDF_USB_PIPE_INFORMATION            pipeInfo;
    ULONG                               waitWakeEnable;
    UCHAR                               numberConfiguredPipes;
    UCHAR                               index;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = STATUS_SUCCESS;
    pDeviceContext = DeviceGetContext(Device);

    // 1. ���� USB �豸������Ա����ǿ�����ײ� USB ��ջ����ͨ�š�WDFUSBDEVICE ������ڲ�ѯ�����ú͹��� USB �豸�ĸ������档
    // ��Щ��������豸���ԡ����������Լ� I/O ������ͬ�������ǽ��ڵ�һ�ε��� PrepareHardware ʱ�����豸��
    // ��� pnp ���������������豸�Խ�����Դ����ƽ�⣬���ǽ�ʹ����ͬ���豸�����Ȼ���ٴ�ѡ��ӿڣ���Ϊ USB ��ջ��������������ʱ���������豸��
    if (pDeviceContext->UsbDevice == NULL) 
    {
        // ָ���ͻ��˰汾 602 ʹ�����ܹ���ѯ��ʹ�� Windows 8 �� USB ���������ջ���¹��ܡ�
        // �⻹��ζ���������� MSDN WdfUsbTargetDeviceCreateWithParameters �ĵ����ᵽ�Ĺ���
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams,
                                         USBD_CLIENT_CONTRACT_VERSION_602
                                         );

        status = WdfUsbTargetDeviceCreateWithParameters(Device,
                                                    &createParams,
                                                    WDF_NO_OBJECT_ATTRIBUTES,
                                                    &pDeviceContext->UsbDevice
                                                    );

        if (!NT_SUCCESS(status)) 
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfUsbTargetDeviceCreateWithParameters failed 0x%x", status);
            return status;
        }
    }

    // 2. ѡ���豸�ĵ�һ�����ã�ʹ��ÿ���ӿڵĵ�һ����������
    //WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_MULTIPLE_INTERFACES(&configParams,
    //                                                             0,
    //                                                             NULL
    //                                                             );
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

    status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->UsbDevice,
                                            WDF_NO_OBJECT_ATTRIBUTES,
                                            &configParams
                                            );

    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "WdfUsbTargetDeviceSelectConfig failed 0x%x", status);
        return status;
    }

    WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);

    // 3. ��ѡ����ѯUSB�豸����Ϣ
    status = WdfUsbTargetDeviceRetrieveInformation(
        pDeviceContext->UsbDevice,
        &deviceInfo);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfUsbTargetDeviceRetrieveInformation failed 0x%x", status);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "IsDeviceHighSpeed: %s\n",
        (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? "TRUE" : "FALSE");
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
        "IsDeviceSelfPowered: %s\n",
        (deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED) ? "TRUE" : "FALSE");

    waitWakeEnable = deviceInfo.Traits &
        WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
        "IsDeviceRemoteWakeable: %s\n",
        waitWakeEnable ? "TRUE" : "FALSE");

    // Save these for use later.
    pDeviceContext->UsbDeviceTraits = deviceInfo.Traits;

    // 4. ����USB�豸�Ķ˵�
    pDeviceContext->UsbInterface =
        configParams.Types.SingleInterface.ConfiguredUsbInterface;
    numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

    for (index = 0; index < numberConfiguredPipes; index++)
    {
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

        pipe = WdfUsbInterfaceGetConfiguredPipe(
            pDeviceContext->UsbInterface,
            index, //PipeIndex,
            &pipeInfo
        );
        // ���߿�ܿ��Զ�ȡС��MaximumPacketSize �����ݰ�
        WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

        if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType) 
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
                "Interrupt Pipe is 0x%p\n", pipe);
            pDeviceContext->InterruptPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsInEndpoint(pipe)) 
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
                "BulkInput Pipe is 0x%p\n", pipe);
            pDeviceContext->BulkReadPipe = pipe;
        }

        if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
            WdfUsbTargetPipeIsOutEndpoint(pipe))
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL,
                "BulkOutput Pipe is 0x%p\n", pipe);
            pDeviceContext->BulkWritePipe = pipe;
        }
    }

    if (!(pDeviceContext->BulkWritePipe && pDeviceContext->BulkReadPipe && pDeviceContext->InterruptPipe)) 
    {
        status = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "Device is not configured properly %!STATUS!\n",
            status);

        return status;
    }

    // 5. ���õ�Դ����
    PL2303SetPowerPolicy(pDeviceContext->Device);

    // 6. ��ʼ��������
    RtlZeroMemory(pDeviceContext->Data, DATA_BUFFER_SIZE);
    AllocBuffer(&pDeviceContext->NMEABuffer, pDeviceContext->Data, DATA_BUFFER_SIZE, pDeviceContext->Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS
USBKmDriverEvtDeviceRelease(
    _In_
    WDFDEVICE Device,
    _In_
    WDFCMRESLIST ResourcesTranslated
)
{
    ResourcesTranslated;
    PDEVICE_CONTEXT pDeviceContext;

    pDeviceContext = DeviceGetContext(Device);

    FreeBuffer(&pDeviceContext->NMEABuffer);

    return STATUS_SUCCESS;
}

NTSTATUS
PL2303EvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
)
/*++
����������

PL2303EvtDeviceD0Entry �¼��ص�����ִ����ʹ��ָ���豸֮ǰ������κβ�����ÿ����Ҫ�����£���ʼ��Ӳ��ʱ�������������
�˺���δ���Ϊ�ɷ�ҳ����Ϊ�˺���λ���豸����·���С����������Ϊ�ɷ�ҳ�Ҵ��벿���ѷ�ҳʱ����������ҳ���������ܻ�Ӱ����ٻָ���Ϊ����Ϊ�ͻ��������������ȴ���ֱ��ϵͳ����������Դ����ҳ�����

�˺����� PASSIVE_LEVEL �����У���ʹ��δ��ҳ����������� DO_POWER_PAGABLE�������������ѡ��ʹ�˺����ɷ�ҳ����ʹδ���� DO_POWER_PAGABLE���˺������� PASSIVE_LEVEL ���С�������������£��ú������Բ���ִ���κλᵼ��ҳ���������顣

������

Device - ����豸����ľ����
PreviousState - �豸����ĵ�Դ״̬������豸���������ģ���ΪPowerDeviceUnspecified��

Return Value:
    NTSTATUS
--*/
{
    PDEVICE_CONTEXT         pDeviceContext;
    NTSTATUS                status;
    BOOLEAN                 isTargetStarted;

    char                    buff[7] = { 0 };

    pDeviceContext = DeviceGetContext(Device);
    isTargetStarted = FALSE;

    PreviousState;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER,
        "-->PL2303EvtDeviceD0Entry -> \n");
    isTargetStarted = TRUE;
    // 1. ���ݶ�����������
    status = WriteConfigPipe(0x0404, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(0x0404, 1, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(0, 1, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(1, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(2, 0x44, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(8, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(9, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    // 2. д�벨���ʵȲ���
    buff[0] = 0x00;
    buff[1] = 0xC2;
    buff[2] = 0x01;
    buff[3] = 0x00;
    buff[6] = 0x08;

    status = SynchControlCommand(CLASS_COMMAND, DEVICE2HOST, 32, 0, 0, buff, 7, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "SynchControlCommand failed 0x%x", status);
        return status;
    }

    status = SynchControlCommand(CLASS_COMMAND, DEVICE2HOST, 34, 0, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "SynchControlCommand failed 0x%x", status);
        return status;
    }

    status = WriteConfigPipe(0x0505, 0x1311, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WriteConfigPipe failed 0x%x", status);
        return status;
    }

    status = SynchControlCommand(CLASS_COMMAND, DEVICE2HOST, 34, 1, 0, NULL, 0, pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "SynchControlCommand failed 0x%x", status);
        return status;
    }

    // 3. �����ں��߳�
    status = CreateKernelThread(&pDeviceContext->DeviceContext);
    if (!NT_SUCCESS(status)) 
    {
        // D0Entry ʧ�ܽ������豸���Ƴ�����ˣ�������ֹͣ������ȡ����Ϊ�����Ƴ���׼����
        if (isTargetStarted) 
        {
            WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptPipe), WdfIoTargetCancelSentIo);
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<--PL2303EvtDeviceD0Entry\n");
    return status;
}


NTSTATUS
PL2303EvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
)
/*++
����������
�����̳����� EvtDeviceD0Entry ��ִ�е��κβ�����ÿ���豸�뿪 D0 ״̬ʱ�������������������������豸ֹͣ���Ƴ��͹رյ�Դʱ��
���ô˻ص�ʱ���豸�Դ��� D0 ״̬������ζ�����������Կ��ڴ������нӴ�Ӳ����

EvtDeviceD0Exit �¼��ص�����ִ����ָ���豸�Ƴ� D0 ״̬֮ǰ������κβ������������������Ҫ���豸�ر�֮ǰ����Ӳ��״̬����Ӧ�ڴ˴���ɡ�
�˺����� PASSIVE_LEVEL ���У���ͨ������ҳ����������� DO_POWER_PAGABLE�������������ѡ��ʹ�˺����ɷ�ҳ��
��ʹδ���� DO_POWER_PAGABLE���˺������� PASSIVE_LEVEL ���С�������������£��ú������Բ���ִ���κλᵼ��ҳ���������顣

������
Device - ����豸����ľ����
TargetState - �˻ص���ɺ��豸�����ڵ��豸��Դ״̬��

Return Value:
    Success implies that the device can be used.  Failure will result in the
    device stack being torn down.
--*/
{
    PDEVICE_CONTEXT         pDeviceContext;

    PAGED_CODE();

    TargetState;

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER,
        "-->OsrFxEvtDeviceD0Exit ->\n");

    pDeviceContext = DeviceGetContext(Device);

    WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptPipe), WdfIoTargetCancelSentIo);

    StopKernalThread(&pDeviceContext->DeviceContext);   

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<--OsrFxEvtDeviceD0Exit\n");

    return STATUS_SUCCESS;
}

VOID
PL2303EvtDeviceSelfManagedIoFlush(
    _In_ WDFDEVICE Device
)
/*++
Routine Description:

�����̴����豸�Թ��� I/O ������ˢ�»��

Arguments:
    Device - Handle to a framework device object.

Return Value:
    None
--*/
{
    // Service the interrupt message queue to drain any outstanding requests
    // OsrUsbIoctlGetInterruptMessage(Device, STATUS_DEVICE_REMOVED);
    Device;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
PL2303SetPowerPolicy(
    _In_ WDFDEVICE Device
)
{
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
    NTSTATUS    status = STATUS_SUCCESS;

    PAGED_CODE();

    //
    // Init the idle policy structure.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
    idleSettings.IdleTimeout = 10000; // 10-sec

    status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "WdfDeviceSetPowerPolicyS0IdlePolicy failed %x\n", status);
        return status;
    }

    // Init wait-wake policy structure.
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

    status = WdfDeviceAssignSxWakeSettings(Device, &wakeSettings);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "WdfDeviceAssignSxWakeSettings failed %x\n", status);
        return status;
    }

    return status;
}
////////////////////////////////////////////////////////////////////
// ��ʼ�������
NTSTATUS AllocBuffer(PNMEABUFFER Self, UCHAR* Base, size_t Size, WDFDEVICE Device)
{
    memset(Self, 0, sizeof(NMEABUFFER));

    Self->pBase = Base;

    Self->Size = Size;
    Self->LastSize = Size;

    WDF_OBJECT_ATTRIBUTES attributes;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;
    NTSTATUS status = WdfWaitLockCreate(&attributes, &Self->lock);
    if (!NT_SUCCESS(status))
    {
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}

// �ͷŻ����
VOID FreeBuffer(PNMEABUFFER Self)
{
    UCHAR* Base = Self->pBase;
    size_t Size = Self->Size;

    memset(Self, 0, sizeof(NMEABUFFER));

    Self->pBase = Base;

    Self->Size = Size;
    Self->LastSize = Size;
}

// ��������
NTSTATUS ReadBuffer(PNMEABUFFER Self, size_t ReadSize, UCHAR* Out, size_t* ReadRet)
{
    if (Self->pBase == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    WdfWaitLockAcquire(Self->lock, NULL);

    if (ReadSize > (Self->WriteOffset - Self->ReadOffset))
    {
        // ��������¶�ȡ�����ݴ��ڿ��õ�����
        ReadSize = (Self->WriteOffset - Self->ReadOffset);
    }

    memcpy(Out, Self->pBase + Self->ReadOffset, ReadSize);

    Self->ReadOffset += ReadSize;

    *ReadRet = ReadSize;

    WdfWaitLockRelease(Self->lock);

    return STATUS_SUCCESS;
}

// д������
NTSTATUS WriteBuffer(PNMEABUFFER Self, size_t WriteSize, UCHAR* pData)
{
    if (Self->pBase == NULL || WriteSize > (Self->Size - 1))
    {
        return STATUS_UNSUCCESSFUL;
    }

    UCHAR* pCopyAddr = NULL;
    WdfWaitLockAcquire(Self->lock, NULL);
    if (WriteSize > Self->LastSize - 1)
    {
        // ��ʱ��ֹ��
        memset(Self->pBase, 0, Self->Size);

        Self->ReadOffset = 0;

        Self->WriteOffset = WriteSize;

        Self->LastSize = Self->Size - WriteSize;

        pCopyAddr = Self->pBase;
    }
    else
    {
        pCopyAddr = Self->pBase + Self->WriteOffset;

        Self->WriteOffset += WriteSize;

        Self->LastSize -= WriteSize;
    }
    memcpy(pCopyAddr, pData, WriteSize);
    WdfWaitLockRelease(Self->lock);

    return STATUS_SUCCESS;
}

// ��ȡ������ʣ���С
size_t GetBufferSize(PNMEABUFFER Self)
{
    if (Self->WriteOffset > Self->ReadOffset)
    {
        return Self->WriteOffset - Self->ReadOffset;
    }

    return 0;
}
////////////////////////////////////////////////////////////////////
// ��0�����ö˵��������
NTSTATUS ReadConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext)
{
    return SynchControlCommand(VENDOR_COMMAND, DEVICE2HOST, CONTROL_COMMAND, value, index, bytes, size, pContext);
}

// ��0�����ö˵��д����
NTSTATUS WriteConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext)
{
    return SynchControlCommand(VENDOR_COMMAND, HOST2DEVICE, CONTROL_COMMAND, value, index, bytes, size, pContext);
}

NTSTATUS SynchControlCommand(int type, int Direction, int request, int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext)
{
    NTSTATUS                        status;
    WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
    WDF_REQUEST_SEND_OPTIONS        sendOptions;
    WDF_MEMORY_DESCRIPTOR           memoryDescriptor;

    // 1. ��ʼ���ڴ�����
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, (PVOID)bytes, size);

    // 2.���ó�ʱʱ��
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(3));

    // 3. ��ʼ����������
    if (type == VENDOR_COMMAND)
    {
        WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
            BmRequestHostToDevice,       // Direction of the request
            Direction,                  // Recipient
            (BYTE)request,              // Vendor command
            (USHORT)value,              // Value
            (USHORT)index);             // Index 
    }
    else
    {
        WDF_USB_CONTROL_SETUP_PACKET_INIT_CLASS(&controlSetupPacket,
            BmRequestHostToDevice,       // Direction of the request
            Direction,                  // Recipient
            (BYTE)request,              // class command
            (USHORT)value,              // Value
            (USHORT)index);             // Index 
    }

    // 4. ͬ����������
    status = WdfUsbTargetDeviceSendControlTransferSynchronously(
        pContext->UsbDevice,
        WDF_NO_HANDLE,               // Optional WDFREQUEST
        &sendOptions,
        &controlSetupPacket,
        &memoryDescriptor,           // MemoryDescriptor
        NULL);                       // BytesTransferred 

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
            "WdfUsbTargetDeviceSendControlTransferSynchronously failed %x\n", status);
    }

    return status;
}

NTSTATUS SynchBulkRead(char* bytes, int size, ULONG *pRetSize, PDEVICE_CONTEXT pContext)
{
    NTSTATUS status = -1;
    WDF_MEMORY_DESCRIPTOR  readBufDesc;

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
        &readBufDesc,
        bytes,
        size
    );

    status = WdfUsbTargetPipeReadSynchronously(
        pContext->BulkReadPipe,
        NULL,
        NULL,
        &readBufDesc,
        pRetSize
    );

    if (!NT_SUCCESS(status) || (int)*pRetSize > size)
    {
        return status;
    }

    return status;
}

NTSTATUS CreateKernelThread(PThreadContext pContext)
{
    NTSTATUS status;

    pContext->ExitEvent = ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KEVENT), 'KBSU');
    if (NULL == pContext->ExitEvent)
    {
        return STATUS_UNSUCCESSFUL;
    }

    KeInitializeEvent(pContext->ExitEvent, SynchronizationEvent, FALSE);

    // �����ں��߳�
    status = PsCreateSystemThread(
        &pContext->ThreadHandle,  // ���ص��߳̾��
        THREAD_ALL_ACCESS,        // �̵߳ķ���Ȩ��
        NULL,                     // ��������
        NULL,                     // �߳������Ľ��� (NULL ��ʾϵͳ����)
        NULL,                     // ClientId (����Ҫ)
        ThreadRoutine,            // �߳���ں���
        pContext                  // ���ݸ��̵߳������Ĳ���
    );

    if (!NT_SUCCESS(status)) 
    {
        ExFreePoolWithTag(pContext->ExitEvent, 'KBSU');
        pContext->ExitEvent = NULL;

        return status;
    }

    // ����������
    status = ObReferenceObjectByHandle(
        pContext->ThreadHandle,
        THREAD_ALL_ACCESS,
        *PsThreadType,
        KernelMode,
        &pContext->ThreadObject,
        NULL);
    if (!NT_SUCCESS(status))
    {
        PsTerminateSystemThread(status);
        pContext->ThreadHandle = NULL;

        ExFreePoolWithTag(pContext->ExitEvent, 'KBSU');
        pContext->ExitEvent = NULL;

        return status;
    }

    return status;
}

NTSTATUS StopKernalThread(PThreadContext pContext)
{
    NTSTATUS status = -1;
    if (pContext->ExitEvent != NULL)
    {
        KeSetEvent(pContext->ExitEvent, 0, FALSE);

        if (pContext->ThreadObject != NULL)
        {
            LARGE_INTEGER timeout;

            // ���ó�ʱʱ�䣨���� 1 �룩
            timeout.QuadPart = ( - 10 * 1000 * 1000); // 1 �룬��λΪ 100 ����

            // �ȴ��¼���ʱ
            status = KeWaitForSingleObject(
                pContext->ThreadObject, // �ȴ��Ķ���
                Executive,              // �ȴ�����
                KernelMode,             // ����ģʽ
                FALSE,                  // �Ƿ�ɾ���
                &timeout                // ��ʱʱ��
            );

            if (status == STATUS_SUCCESS) 
            {
                ZwClose(pContext->ThreadHandle);
                ObDereferenceObject(pContext->ThreadObject);

                // ����¼������ã��˳�ѭ��
                pContext->ThreadObject = NULL;
                pContext->ThreadHandle = NULL;
            }
        }
        ExFreePoolWithTag(pContext->ExitEvent, 'KBSU');
        pContext->ExitEvent = NULL;
        return status;
    }
    return status;
}

VOID ThreadRoutine(PVOID Context)
{
    PThreadContext  pThreadContext = (PThreadContext)Context;
    PDEVICE_CONTEXT pDeviceContext = CONTAINING_RECORD(pThreadContext, DEVICE_CONTEXT, DeviceContext);

    WDFQUEUE queue = WdfDeviceGetDefaultQueue(pDeviceContext->Device);
    PQUEUE_CONTEXT queueContext = QueueGetContext(queue);
    WDFREQUEST savedRequest;

    char buff[BUFF_SIZE] = { 0 };
    ULONG ulSize = 0;
    NTSTATUS status = -1;
    LARGE_INTEGER timeout;

    // ���ó�ʱʱ��
    timeout.QuadPart = (-10 * 1000 * 50); // 1 �룬��λΪ 100 ����

    while (TRUE)
    {
        // �ȴ��¼���ʱ
        status = KeWaitForSingleObject(
            pThreadContext->ExitEvent, // �ȴ��Ķ���
            Executive,                  // �ȴ�����
            KernelMode,                 // ����ģʽ
            FALSE,                      // �Ƿ�ɾ���
            &timeout                    // ��ʱʱ��
        );
        if (status == STATUS_SUCCESS)
        {
            break;
        }

        SynchBulkRead(buff, BUFF_SIZE, &ulSize, pDeviceContext);

        WriteBuffer(&pDeviceContext->NMEABuffer, ulSize, (UCHAR*)buff);

        RtlZeroMemory(buff, BUFF_SIZE);

        // �жϵײ��Ƿ���ڵȴ��Ķ���
        status = WdfIoQueueRetrieveNextRequest(
            queueContext->WaitMaskQueue,
            &savedRequest);
        if (!NT_SUCCESS(status)) 
        {
            continue;
        }

        status = WdfRequestForwardToIoQueue(
            savedRequest,
            queueContext->Queue);
        if (!NT_SUCCESS(status))
        {
            continue;
        }

        queueContext->MaskEvent = 0x1;
    }

    // �߳������˳�
    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS CreateSymbolName(WDFDEVICE Device)
{
    NTSTATUS status = 0;

    UNICODE_STRING portName;
    UNICODE_STRING comPort;
    UNICODE_STRING symbolicLinkName;

    WDFKEY  key;

    WCHAR   Buffer[COM_NAME_SIZE] = { 0 };
    WCHAR   NameBuffer[SYMBOL_NAME_SIZE] = { 0 };

    LPGUID  guid = (LPGUID)&GUID_DEVINTERFACE_COMPORT;

    // 1. �����豸�ӿ�
    status = WdfDeviceCreateDeviceInterface(
        Device, 
        guid, 
        NULL);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    // 2. ��ע����ж�ȡCOM�˿ں�,���INF�ļ���ʾClass=Ports������MsPorts!PortsClassInstaller����
    status = WdfDeviceOpenRegistryKey(
        Device,
        PLUGPLAY_REGKEY_DEVICE,
        KEY_QUERY_VALUE,
        WDF_NO_OBJECT_ATTRIBUTES,
        &key);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    RtlInitUnicodeString(
        &portName, 
        L"PortName");

    RTL_INIT_UNICODE(comPort, 0, Buffer, COM_NAME_SIZE);

    status = WdfRegistryQueryUnicodeString(
        key,
        &portName,
        NULL,
        &comPort);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    RTL_INIT_UNICODE(symbolicLinkName, 0, NameBuffer, SYMBOL_NAME_SIZE);

    RtlUnicodeStringPrintf(
        &symbolicLinkName, 
        L"%s%s", 
        L"\\DosDevices\\Global\\", 
        comPort.Buffer);

    // 3. ������������
    status = WdfDeviceCreateSymbolicLink(
        Device, 
        &symbolicLinkName);
Exit:
    return status;
}