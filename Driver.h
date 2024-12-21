/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <usbdlib.h>
#include <wdfusb.h>
#include <initguid.h>

#include <Ntstrsafe.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD USBKmDriverEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP USBKmDriverEvtDriverContextCleanup;

typedef
NTSTATUS
(*PFN_IO_GET_ACTIVITY_ID_IRP) (
    _In_     PIRP   Irp,
    _Out_    LPGUID Guid
    );

typedef
NTSTATUS
(*PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA) (
    _In_ PUNICODE_STRING    SymbolicLinkName,
    _In_ CONST DEVPROPKEY* PropertyKey,
    _In_ LCID               Lcid,
    _In_ ULONG              Flags,
    _In_ DEVPROPTYPE        Type,
    _In_ ULONG              Size,
    _In_opt_ PVOID          Data
    );

extern PFN_IO_GET_ACTIVITY_ID_IRP g_pIoGetActivityIdIrp;

extern PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA g_pIoSetDeviceInterfacePropertyData;

EXTERN_C_END
