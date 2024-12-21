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
例程描述：调用工作程序例程来创建设备及其软件资源。

参数：
DeviceInit - 指向不透明初始化结构的指针。当 WdfDeviceCreate 成功时，框架将释放此结构的内存。因此，此后不要访问该结构。

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

        // 获取指向我们刚刚与设备对象关联的设备上下文结构的指针。我们在 device.h
        // 头文件中定义此结构。DeviceGetContext 是通过使用 device.h 中的 WDF_DECLARE_CONTEXT_TYPE_WITH_NAME 宏生成的内联函数。
        // 此函数将执行类型检查并返回设备上下文。如果您传递了错误的对象句柄，它将返回 NULL 并断言是否在框架验证器模式下运行。
        deviceContext = DeviceGetContext(device);

        // 初始化设备扩展
        deviceContext->PrivateDeviceData = 0;
        deviceContext->Device = device;

        status = CreateSymbolName(device);
        if (NT_SUCCESS(status)) 
        {
            // 初始化 I/O 和任何队列
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
例程描述：在此回调中，驱动程序将执行一切必要操作以使硬件准备就绪。对于 USB 设备，这涉及读取和选择描述符。

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

    // 1. 创建 USB 设备句柄，以便我们可以与底层 USB 堆栈进行通信。WDFUSBDEVICE 句柄用于查询、配置和管理 USB 设备的各个方面。
    // 这些方面包括设备属性、总线属性以及 I/O 创建和同步。我们仅在第一次调用 PrepareHardware 时创建设备。
    // 如果 pnp 管理器重新启动设备以进行资源重新平衡，我们将使用相同的设备句柄，然后再次选择接口，因为 USB 堆栈可以在重新启动时重新配置设备。
    if (pDeviceContext->UsbDevice == NULL) 
    {
        // 指定客户端版本 602 使我们能够查询并使用 Windows 8 的 USB 驱动程序堆栈的新功能。
        // 这还意味着我们遵守 MSDN WdfUsbTargetDeviceCreateWithParameters 文档中提到的规则。
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

    // 2. 选择设备的第一个配置，使用每个接口的第一个备用设置
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

    // 3. 可选，查询USB设备的信息
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

    // 4. 保存USB设备的端点
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
        // 告诉框架可以读取小于MaximumPacketSize 的数据包
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

    // 5. 设置电源属性
    PL2303SetPowerPolicy(pDeviceContext->Device);

    // 6. 初始化缓冲区
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
例程描述：

PL2303EvtDeviceD0Entry 事件回调必须执行在使用指定设备之前所需的任何操作。每次需要（重新）初始化硬件时，都会调用它。
此函数未标记为可分页，因为此函数位于设备启动路径中。当函数标记为可分页且代码部分已分页时，它将生成页面错误，这可能会影响快速恢复行为，因为客户端驱动程序必须等待，直到系统驱动程序可以处理此页面错误。

此函数在 PASSIVE_LEVEL 上运行，即使它未分页。如果设置了 DO_POWER_PAGABLE，驱动程序可以选择使此函数可分页。即使未设置 DO_POWER_PAGABLE，此函数仍以 PASSIVE_LEVEL 运行。但在这种情况下，该函数绝对不能执行任何会导致页面错误的事情。

参数：

Device - 框架设备对象的句柄。
PreviousState - 设备最近的电源状态。如果设备是新启动的，则为PowerDeviceUnspecified。

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
    // 1. 根据定义设置配置
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

    // 2. 写入波特率等参数
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

    // 3. 启动内核线程
    status = CreateKernelThread(&pDeviceContext->DeviceContext);
    if (!NT_SUCCESS(status)) 
    {
        // D0Entry 失败将导致设备被移除。因此，让我们停止连续读取器，为随后的移除做准备。
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
例程描述：
此例程撤消在 EvtDeviceD0Entry 中执行的任何操作。每当设备离开 D0 状态时都会调用它，这种情况发生在设备停止、移除和关闭电源时。
调用此回调时，设备仍处于 D0 状态，这意味着驱动程序仍可在此例程中接触硬件。

EvtDeviceD0Exit 事件回调必须执行在指定设备移出 D0 状态之前所需的任何操作。如果驱动程序需要在设备关闭之前保存硬件状态，则应在此处完成。
此函数在 PASSIVE_LEVEL 运行，但通常不分页。如果设置了 DO_POWER_PAGABLE，驱动程序可以选择使此函数可分页。
即使未设置 DO_POWER_PAGABLE，此函数仍在 PASSIVE_LEVEL 运行。但在这种情况下，该函数绝对不能执行任何会导致页面错误的事情。

参数：
Device - 框架设备对象的句柄。
TargetState - 此回调完成后设备将处于的设备电源状态。

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

此例程处理设备自管理 I/O 操作的刷新活动。

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
// 初始化缓冲池
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

// 释放缓冲池
VOID FreeBuffer(PNMEABUFFER Self)
{
    UCHAR* Base = Self->pBase;
    size_t Size = Self->Size;

    memset(Self, 0, sizeof(NMEABUFFER));

    Self->pBase = Base;

    Self->Size = Size;
    Self->LastSize = Size;
}

// 读缓冲区
NTSTATUS ReadBuffer(PNMEABUFFER Self, size_t ReadSize, UCHAR* Out, size_t* ReadRet)
{
    if (Self->pBase == NULL)
    {
        return STATUS_UNSUCCESSFUL;
    }

    WdfWaitLockAcquire(Self->lock, NULL);

    if (ReadSize > (Self->WriteOffset - Self->ReadOffset))
    {
        // 这种情况下读取的数据大于可用的数据
        ReadSize = (Self->WriteOffset - Self->ReadOffset);
    }

    memcpy(Out, Self->pBase + Self->ReadOffset, ReadSize);

    Self->ReadOffset += ReadSize;

    *ReadRet = ReadSize;

    WdfWaitLockRelease(Self->lock);

    return STATUS_SUCCESS;
}

// 写缓冲区
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
        // 此时禁止读
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

// 获取缓冲区剩余大小
size_t GetBufferSize(PNMEABUFFER Self)
{
    if (Self->WriteOffset > Self->ReadOffset)
    {
        return Self->WriteOffset - Self->ReadOffset;
    }

    return 0;
}
////////////////////////////////////////////////////////////////////
// 对0号配置端点进读操作
NTSTATUS ReadConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext)
{
    return SynchControlCommand(VENDOR_COMMAND, DEVICE2HOST, CONTROL_COMMAND, value, index, bytes, size, pContext);
}

// 对0号配置端点进写操作
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

    // 1. 初始化内存区域
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, (PVOID)bytes, size);

    // 2.设置超时时间
    WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_SEC(3));

    // 3. 初始化请求类型
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

    // 4. 同步发送请求
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

    // 创建内核线程
    status = PsCreateSystemThread(
        &pContext->ThreadHandle,  // 返回的线程句柄
        THREAD_ALL_ACCESS,        // 线程的访问权限
        NULL,                     // 对象属性
        NULL,                     // 线程隶属的进程 (NULL 表示系统进程)
        NULL,                     // ClientId (不需要)
        ThreadRoutine,            // 线程入口函数
        pContext                  // 传递给线程的上下文参数
    );

    if (!NT_SUCCESS(status)) 
    {
        ExFreePoolWithTag(pContext->ExitEvent, 'KBSU');
        pContext->ExitEvent = NULL;

        return status;
    }

    // 解析对象句柄
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

            // 设置超时时间（例如 1 秒）
            timeout.QuadPart = ( - 10 * 1000 * 1000); // 1 秒，单位为 100 纳秒

            // 等待事件或超时
            status = KeWaitForSingleObject(
                pContext->ThreadObject, // 等待的对象
                Executive,              // 等待类型
                KernelMode,             // 运行模式
                FALSE,                  // 是否可警报
                &timeout                // 超时时间
            );

            if (status == STATUS_SUCCESS) 
            {
                ZwClose(pContext->ThreadHandle);
                ObDereferenceObject(pContext->ThreadObject);

                // 如果事件被设置，退出循环
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

    // 设置超时时间
    timeout.QuadPart = (-10 * 1000 * 50); // 1 秒，单位为 100 纳秒

    while (TRUE)
    {
        // 等待事件或超时
        status = KeWaitForSingleObject(
            pThreadContext->ExitEvent, // 等待的对象
            Executive,                  // 等待类型
            KernelMode,                 // 运行模式
            FALSE,                      // 是否可警报
            &timeout                    // 超时时间
        );
        if (status == STATUS_SUCCESS)
        {
            break;
        }

        SynchBulkRead(buff, BUFF_SIZE, &ulSize, pDeviceContext);

        WriteBuffer(&pDeviceContext->NMEABuffer, ulSize, (UCHAR*)buff);

        RtlZeroMemory(buff, BUFF_SIZE);

        // 判断底层是否存在等待的队列
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

    // 线程正常退出
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

    // 1. 创建设备接口
    status = WdfDeviceCreateDeviceInterface(
        Device, 
        guid, 
        NULL);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    // 2. 从注册表中读取COM端口号,如果INF文件显示Class=Ports，则由MsPorts!PortsClassInstaller创建
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

    // 3. 创建符号链接
    status = WdfDeviceCreateSymbolicLink(
        Device, 
        &symbolicLinkName);
Exit:
    return status;
}