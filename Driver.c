/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, USBKmDriverEvtDeviceAdd)
#pragma alloc_text (PAGE, USBKmDriverEvtDriverContextCleanup)
#endif

PFN_IO_GET_ACTIVITY_ID_IRP g_pIoGetActivityIdIrp;
PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA g_pIoSetDeviceInterfacePropertyData;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++
����˵����
DriverEntry ��ʼ����������������������غ�ϵͳ���õĵ�һ�����̡�DriverEntry ָ���������������е�������ڵ㣬���� EvtDevice �� DriverUnload��

����˵����

DriverObject - ��ʾ���ص��ڴ��еĹ������������ʵ����DriverEntry �����ʼ�� DriverObject �ĳ�Ա��Ȼ����ܷ��ظ������ߡ�DriverObject ��ϵͳ�������������ǰ���䣬����ϵͳ���ڴ���ж�ع��������������ϵͳ�ͷš�
RegistryPath - ��ʾע����е����������ض�·�������������������ʹ�ø�·������������֮��洢��������������ݡ���·�����洢Ӳ��ʵ���ض����ݡ�

Return Value:
    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.
--*/
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    UNICODE_STRING          funcName;

    // Initialize WPP Tracing
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    RtlInitUnicodeString(&funcName, L"IoGetActivityIdIrp");
    g_pIoGetActivityIdIrp = (PFN_IO_GET_ACTIVITY_ID_IRP)(ULONG_PTR)
        MmGetSystemRoutineAddress(&funcName);

    RtlInitUnicodeString(&funcName, L"IoSetDeviceInterfacePropertyData");
    g_pIoSetDeviceInterfacePropertyData = (PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA)(ULONG_PTR)
        MmGetSystemRoutineAddress(&funcName);

    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = USBKmDriverEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, USBKmDriverEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) 
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS
USBKmDriverEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++
������������ܵ��� EvtDeviceAdd ����Ӧ���� PnP �������� AddDevice���á����Ǵ�������ʼ��һ���豸��������ʾ�豸����ʵ����

������
Driver - �� DriverEntry �д����Ŀ�������������ľ��
DeviceInit - ָ���ܷ���� WDFDEVICE_INIT �ṹ��ָ��

Return Value:
    NTSTATUS
--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = USBKmDriverCreateDevice(DeviceInit);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

    return status;
}

VOID
USBKmDriverEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE ();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP( WdfDriverWdmGetDriverObject( (WDFDRIVER) DriverObject) );

}
