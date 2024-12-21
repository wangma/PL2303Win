/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"

EXTERN_C_START

#pragma region 下面的定义是用于缓冲区的结构和操作函数
typedef struct tagNMEABUFFER
{
    UCHAR* pBase;// 基地址，初始化时候分配

    size_t  ReadOffset;// 读指针
    size_t  WriteOffset;// 写指针偏移

    size_t   Size;// 总大小
    size_t   LastSize;// 剩余大小

    WDFWAITLOCK lock;
}NMEABUFFER, * PNMEABUFFER;

// 初始化缓冲池
NTSTATUS AllocBuffer(PNMEABUFFER Self, UCHAR* Base, size_t Size, WDFDEVICE Device);

// 释放缓冲池
VOID FreeBuffer(PNMEABUFFER Self);

// 读缓冲区
NTSTATUS ReadBuffer(PNMEABUFFER Self, size_t ReadSize, UCHAR* Out, size_t* ReadRet);

// 写缓冲区
NTSTATUS WriteBuffer(PNMEABUFFER Self, size_t WriteSize, UCHAR* pLogItem);

// 获取缓冲区的大小
size_t GetBufferSize(PNMEABUFFER Self);
#pragma endregion

#pragma region 内核线程结构定义
typedef struct _tagThreadContext
{
    PKEVENT     ExitEvent;
    PKTHREAD    ThreadObject;
    HANDLE      ThreadHandle;
}ThreadContext,*PThreadContext;

NTSTATUS CreateKernelThread(PThreadContext pContext);

NTSTATUS StopKernalThread(PThreadContext pContext);

VOID ThreadRoutine(PVOID Context);

// 内核读取硬件数据的默认大小
#define BUFF_SIZE 256
#pragma endregion

#pragma region 设备扩展和设备使用的例程
// 定义设备扩展和获取扩展的方式
typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE                       Device;

    WDFUSBDEVICE                    UsbDevice;

    WDFUSBINTERFACE                 UsbInterface;

    WDFUSBPIPE                      BulkReadPipe;

    WDFUSBPIPE                      BulkWritePipe;

    WDFUSBPIPE                      InterruptPipe;

    ULONG                           UsbDeviceTraits;

    NMEABUFFER                      NMEABuffer;

    UCHAR                           Data[DATA_BUFFER_SIZE];

    ThreadContext                   DeviceContext;

    ULONG PrivateDeviceData;  // just a placeholder

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS
USBKmDriverCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

EVT_WDF_DEVICE_PREPARE_HARDWARE USBKmDriverEvtDevicePrepareHardware;

EVT_WDF_DEVICE_RELEASE_HARDWARE USBKmDriverEvtDeviceRelease;

NTSTATUS
PL2303EvtDeviceD0Entry(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE PreviousState
);

NTSTATUS
PL2303EvtDeviceD0Exit(
    WDFDEVICE Device,
    WDF_POWER_DEVICE_STATE TargetState
);

VOID
PL2303EvtDeviceSelfManagedIoFlush(
    _In_ WDFDEVICE Device
);

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
PL2303SetPowerPolicy(
    _In_ WDFDEVICE Device
);
#pragma endregion

#pragma region PL2303芯片对应的宏定义和函数定义
// 主机写数据
#define HOST2DEVICE 0
// 主机读数据
#define DEVICE2HOST 1

// 厂家定义命令
#define VENDOR_COMMAND 0
// USB协议定义的命令
#define CLASS_COMMAND 1
// 控制类命令
#define CONTROL_COMMAND 1

// 对0号配置端点进读操作
NTSTATUS ReadConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext);

// 对0号配置端点进写操作
NTSTATUS WriteConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext);

// 以同步的方式发送控制命令
NTSTATUS SynchControlCommand(int type, int Direction, int request, int value, int index, char *bytes, int size, PDEVICE_CONTEXT pContext);

// 以同步的方式发起Bulk Read
NTSTATUS SynchBulkRead(char *bytes, int size, ULONG *ulRetSize, PDEVICE_CONTEXT pContext);

// COM接口类GUID
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0X86E0D1E0L, 0X8089, 0X11D0, 0X9C, 0XE4, 0X08, 0X00, 0X3E, 0X30, 0X1F, 0X73);

// 创建符号名称
NTSTATUS CreateSymbolName(WDFDEVICE Device);
#pragma endregion
EXTERN_C_END
