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
����������
�ڴ˺��������ÿ���豸����� I/O ���Ȼص������õ���Ĭ�� I/O �����Խ��в������������������������������ڴ�����Ա������ǵĽṹ QUEUE_CONTEXT��

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

    // ����һ��Ĭ�϶��У��Ա�δʹ�� WdfDeviceConfigureRequestDispatching ����ת�����������е���������ڴ˴����ɡ�
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

    // �ڴ��������£�EvtIoStop �ص���������ɡ�ȡ�����Ƴٶ� I/O ����Ľ�һ������

    // ͨ������������ʹ�����¹���

    // - �����������ӵ�� I/O ��������Ҫô�Ƴٶ�����Ľ�һ���������� WdfRequestStopAcknowledge��Ҫô���� WdfRequestComplete���������״ֵ̬����Ϊ STATUS_SUCCESS �� STATUS_CANCELLED��
    // ���������������� WdfRequestComplete һ�Σ�����ɻ�ȡ������Ϊȷ����һ���̲߳����ͬһ������� WdfRequestComplete��EvtIoStop �ص���������������������¼��ص�����ͬ��������ͨ��ʹ�û���������

    // - ������������ѽ� I/O ����ת���� I/O Ŀ�꣬����Ҫô����
    // WdfRequestCancelSentRequest ������ȡ��������Ҫô�Ƴ�
    // �Ը�����Ľ�һ���������� WdfRequestStopAcknowledge��

    // �����������ѡ���� EvtIoStop �в��Ա�֤�ںܶ�ʱ������ɵ������ȡ�κδ�ʩ�����磬�����������
    // ����������������������֮һ����ɵ����󲻲�ȡ�κδ�ʩ��
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