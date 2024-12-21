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
例程说明：
DriverEntry 初始化驱动程序，是驱动程序加载后系统调用的第一个例程。DriverEntry 指定功能驱动程序中的其他入口点，例如 EvtDevice 和 DriverUnload。

参数说明：

DriverObject - 表示加载到内存中的功能驱动程序的实例。DriverEntry 必须初始化 DriverObject 的成员，然后才能返回给调用者。DriverObject 由系统在驱动程序加载前分配，并在系统从内存中卸载功能驱动程序后由系统释放。
RegistryPath - 表示注册表中的驱动程序特定路径。功能驱动程序可以使用该路径在重新启动之间存储驱动程序相关数据。该路径不存储硬件实例特定数据。

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
例程描述：框架调用 EvtDeviceAdd 来响应来自 PnP 管理器的 AddDevice调用。我们创建并初始化一个设备对象来表示设备的新实例。

参数：
Driver - 在 DriverEntry 中创建的框架驱动程序对象的句柄
DeviceInit - 指向框架分配的 WDFDEVICE_INIT 结构的指针

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
