/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/
#pragma region �궨��
// ��������С���趨Ϊһ���ڴ�ҳ
#define DATA_BUFFER_SIZE 4096

// COM�ڵ����Ƴ���
#define COM_NAME_SIZE 16

// �������Ƴ���
#define SYMBOL_NAME_SIZE 64

// ��ʼ��unicode�ַ���
#define RTL_INIT_UNICODE(_obj, _length, _buff, _max_size) \
            {   \
                _obj.Length = _length;\
                _obj.Buffer = _buff; \
                _obj.MaximumLength = _max_size;\
            }
#pragma endregion