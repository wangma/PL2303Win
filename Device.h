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

#pragma region ����Ķ��������ڻ������Ľṹ�Ͳ�������
typedef struct tagNMEABUFFER
{
    UCHAR* pBase;// ����ַ����ʼ��ʱ�����

    size_t  ReadOffset;// ��ָ��
    size_t  WriteOffset;// дָ��ƫ��

    size_t   Size;// �ܴ�С
    size_t   LastSize;// ʣ���С

    WDFWAITLOCK lock;
}NMEABUFFER, * PNMEABUFFER;

// ��ʼ�������
NTSTATUS AllocBuffer(PNMEABUFFER Self, UCHAR* Base, size_t Size, WDFDEVICE Device);

// �ͷŻ����
VOID FreeBuffer(PNMEABUFFER Self);

// ��������
NTSTATUS ReadBuffer(PNMEABUFFER Self, size_t ReadSize, UCHAR* Out, size_t* ReadRet);

// д������
NTSTATUS WriteBuffer(PNMEABUFFER Self, size_t WriteSize, UCHAR* pLogItem);

// ��ȡ�������Ĵ�С
size_t GetBufferSize(PNMEABUFFER Self);
#pragma endregion

#pragma region �ں��߳̽ṹ����
typedef struct _tagThreadContext
{
    PKEVENT     ExitEvent;
    PKTHREAD    ThreadObject;
    HANDLE      ThreadHandle;
}ThreadContext,*PThreadContext;

NTSTATUS CreateKernelThread(PThreadContext pContext);

NTSTATUS StopKernalThread(PThreadContext pContext);

VOID ThreadRoutine(PVOID Context);

// �ں˶�ȡӲ�����ݵ�Ĭ�ϴ�С
#define BUFF_SIZE 256
#pragma endregion

#pragma region �豸��չ���豸ʹ�õ�����
// �����豸��չ�ͻ�ȡ��չ�ķ�ʽ
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

#pragma region PL2303оƬ��Ӧ�ĺ궨��ͺ�������
// ����д����
#define HOST2DEVICE 0
// ����������
#define DEVICE2HOST 1

// ���Ҷ�������
#define VENDOR_COMMAND 0
// USBЭ�鶨�������
#define CLASS_COMMAND 1
// ����������
#define CONTROL_COMMAND 1

// ��0�����ö˵��������
NTSTATUS ReadConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext);

// ��0�����ö˵��д����
NTSTATUS WriteConfigPipe(int value, int index, char* bytes, int size, PDEVICE_CONTEXT pContext);

// ��ͬ���ķ�ʽ���Ϳ�������
NTSTATUS SynchControlCommand(int type, int Direction, int request, int value, int index, char *bytes, int size, PDEVICE_CONTEXT pContext);

// ��ͬ���ķ�ʽ����Bulk Read
NTSTATUS SynchBulkRead(char *bytes, int size, ULONG *ulRetSize, PDEVICE_CONTEXT pContext);

// COM�ӿ���GUID
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT, 0X86E0D1E0L, 0X8089, 0X11D0, 0X9C, 0XE4, 0X08, 0X00, 0X3E, 0X30, 0X1F, 0X73);

// ������������
NTSTATUS CreateSymbolName(WDFDEVICE Device);
#pragma endregion
EXTERN_C_END
