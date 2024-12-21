/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, USBKmDriverQueueInitialize)
#endif

NTSTATUS
USBKmDriverQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++
例程描述：
在此函数中配置框架设备对象的 I/O 调度回调。配置单个默认 I/O 队列以进行并行请求处理，并创建驱动程序上下文内存分配以保存我们的结构 QUEUE_CONTEXT。

Arguments:
    Device - Handle to a framework device object.

Return Value:
    VOID
--*/
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG    queueConfig;
    WDF_OBJECT_ATTRIBUTES   queueAttributes;
    PQUEUE_CONTEXT           pQueueContext;

    PAGED_CODE();

    // 配置一个默认队列，以便未使用 WdfDeviceConfigureRequestDispatching 配置转发到其他队列的请求可以在此处分派。
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
         &queueConfig,
        WdfIoQueueDispatchParallel
        );

    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;
    queueConfig.EvtIoStop = USBKmDriverEvtIoStop;
    queueConfig.EvtIoRead = EvtIoRead;
    queueConfig.EvtIoWrite = EvtIoWrite;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &queueAttributes,
        QUEUE_CONTEXT);

    status = WdfIoQueueCreate(
                 Device,
                 &queueConfig,
                 &queueAttributes,
                 &queue
                 );

    if( !NT_SUCCESS(status) ) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    pQueueContext = QueueGetContext(queue);
    pQueueContext->Queue = queue;

    WDF_IO_QUEUE_CONFIG_INIT(
        &queueConfig,
        WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    pQueueContext->ReadQueue = queue;

    WDF_IO_QUEUE_CONFIG_INIT(
        &queueConfig,
        WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

    pQueueContext->WaitMaskQueue = queue;
    pQueueContext->MaskEvent = 0;
    pQueueContext->DeviceContext = DeviceGetContext(Device);
    RtlZeroMemory(&pQueueContext->ComStat, sizeof(COMSTAT));

    return status;
}

VOID
USBKmDriverEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    // 在大多数情况下，EvtIoStop 回调函数会完成、取消或推迟对 I/O 请求的进一步处理。

    // 通常，驱动程序使用以下规则：

    // - 如果驱动程序拥有 I/O 请求，则它要么推迟对请求的进一步处理并调用 WdfRequestStopAcknowledge，要么调用 WdfRequestComplete，并将完成状态值设置为 STATUS_SUCCESS 或 STATUS_CANCELLED。
    // 驱动程序必须仅调用 WdfRequestComplete 一次，以完成或取消请求。为确保另一个线程不会对同一请求调用 WdfRequestComplete，EvtIoStop 回调必须与驱动程序的其他事件回调函数同步，例如通过使用互锁操作。

    // - 如果驱动程序已将 I/O 请求转发到 I/O 目标，则它要么调用
    // WdfRequestCancelSentRequest 来尝试取消该请求，要么推迟
    // 对该请求的进一步处理并调用 WdfRequestStopAcknowledge。

    // 驱动程序可能选择在 EvtIoStop 中不对保证在很短时间内完成的请求采取任何措施。例如，驱动程序可能
    // 对在驱动程序的请求处理程序之一中完成的请求不采取任何措施。
    return;
}

NTSTATUS
RequestCopyFromBuffer(
    _In_  WDFREQUEST        Request,
    _In_  PVOID             SourceBuffer,
    _In_  size_t            NumBytesToCopyFrom
)
{
    NTSTATUS                status;
    WDFMEMORY               memory;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0,
        SourceBuffer, NumBytesToCopyFrom);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    return status;
}

NTSTATUS
RequestCopyToBuffer(
    _In_  WDFREQUEST        Request,
    _In_  PVOID             DestinationBuffer,
    _In_  size_t            NumBytesToCopyTo
)
{
    NTSTATUS                status;
    WDFMEMORY               memory;

    status = WdfRequestRetrieveInputMemory(Request, &memory);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfMemoryCopyToBuffer(memory, 0,
        DestinationBuffer, NumBytesToCopyTo);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WdfRequestSetInformation(Request, NumBytesToCopyTo);
    return status;
}

VOID
EvtIoDeviceControl(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
{
    NTSTATUS                status = 0;
    PQUEUE_CONTEXT          queueContext = QueueGetContext(Queue);
    PDEVICE_CONTEXT         deviceContext = queueContext->DeviceContext;
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //Trace(TRACE_LEVEL_INFO,
    //    "EvtIoDeviceControl 0x%x", IoControlCode);

    switch (IoControlCode)
    {
    case IOCTL_SERIAL_WAIT_ON_MASK:
    {
        if (queueContext->MaskEvent == 0)
        {
            WDFREQUEST savedRequest;

            status = WdfIoQueueRetrieveNextRequest(
                queueContext->WaitMaskQueue,
                &savedRequest);

            if (NT_SUCCESS(status)) {
                WdfRequestComplete(savedRequest,
                    STATUS_UNSUCCESSFUL);
            }

            status = WdfRequestForwardToIoQueue(
                Request,
                queueContext->WaitMaskQueue);

            if (!NT_SUCCESS(status)) {
                Trace(TRACE_LEVEL_ERROR,
                    "Error: WdfRequestForwardToIoQueue failed 0x%x", status);
                WdfRequestComplete(Request, status);
            }
        }
        else
        {
            status = RequestCopyFromBuffer(Request,
                (void*)&queueContext->MaskEvent,
                sizeof(DWORD));

            WdfRequestCompleteWithInformation(Request, status, sizeof(DWORD));

            queueContext->MaskEvent = 0;
        }

        return;
    }

    case IOCTL_SERIAL_GET_COMMSTATUS:
    {
        queueContext->ComStat.cbOutQue = (DWORD)GetBufferSize(&deviceContext->NMEABuffer);
        status = RequestCopyFromBuffer(Request,
            (void*)&queueContext->ComStat,
            sizeof(COMSTAT));

        WdfRequestCompleteWithInformation(Request, status, sizeof(COMSTAT));
        return;
    }

    case IOCTL_SERIAL_SET_WAIT_MASK:
    {
        WDFREQUEST savedRequest;

        status = WdfIoQueueRetrieveNextRequest(queueContext->WaitMaskQueue, &savedRequest);

        if (NT_SUCCESS(status)) {

            ULONG eventMask = 0;
            status = RequestCopyFromBuffer(savedRequest, &eventMask, sizeof(eventMask));

            WdfRequestComplete(savedRequest, status);
        }

        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_SERIAL_SET_BAUD_RATE:
    case IOCTL_SERIAL_GET_BAUD_RATE:
    case IOCTL_SERIAL_SET_MODEM_CONTROL:
    case IOCTL_SERIAL_GET_MODEM_CONTROL:
    case IOCTL_SERIAL_SET_FIFO_CONTROL:
    case IOCTL_SERIAL_GET_LINE_CONTROL:
    case IOCTL_SERIAL_SET_LINE_CONTROL:
    case IOCTL_SERIAL_GET_TIMEOUTS:
    case IOCTL_SERIAL_SET_TIMEOUTS:
    case IOCTL_SERIAL_SET_QUEUE_SIZE:
    case IOCTL_SERIAL_SET_DTR:
    case IOCTL_SERIAL_SET_RTS:
    case IOCTL_SERIAL_CLR_RTS:
    case IOCTL_SERIAL_SET_XON:
    case IOCTL_SERIAL_SET_XOFF:
    case IOCTL_SERIAL_SET_CHARS:
    case IOCTL_SERIAL_GET_CHARS:
    case IOCTL_SERIAL_GET_HANDFLOW:
    case IOCTL_SERIAL_SET_HANDFLOW:
    case IOCTL_SERIAL_RESET_DEVICE:
    case IOCTL_SERIAL_CLR_DTR:
    case IOCTL_SERIAL_PURGE:
    case IOCTL_SERIAL_GET_MODEMSTATUS:
    {
        status = STATUS_SUCCESS;
        break;
    }

    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    // complete the request
    WdfRequestComplete(Request, status);
}

VOID
EvtIoWrite(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            Length
)
{
    NTSTATUS                status;
    PQUEUE_CONTEXT          queueContext = QueueGetContext(Queue);
    PDEVICE_CONTEXT         deviceContext = queueContext->DeviceContext;
    WDFMEMORY               memory;

    status = WdfRequestRetrieveInputMemory(Request, &memory);
    if (!NT_SUCCESS(status))
    {
        return;
    }

    status = WriteBuffer(&deviceContext->NMEABuffer, Length, (PUCHAR)WdfMemoryGetBuffer(memory, NULL));

    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(Request, status);
        return;
    }

    WdfRequestCompleteWithInformation(Request, status, Length);
    return;
}

VOID
EvtIoRead(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            Length
)
{
    NTSTATUS                status;
    PQUEUE_CONTEXT          queueContext = QueueGetContext(Queue);
    PDEVICE_CONTEXT         deviceContext = queueContext->DeviceContext;
    WDFMEMORY               memory;
    size_t                  bytesCopied = 0;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status))
    {
        return ;
    }

    status = ReadBuffer(&deviceContext->NMEABuffer, Length, (BYTE*)WdfMemoryGetBuffer(memory, NULL), &bytesCopied);

    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(Request, status);
        return;
    }
    queueContext->ComStat.cbOutQue -= (DWORD)bytesCopied;
    WdfRequestCompleteWithInformation(Request, status, bytesCopied);

    return;
}