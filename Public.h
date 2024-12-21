/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/
#pragma region 宏定义
// 缓冲区大小，设定为一个内存页
#define DATA_BUFFER_SIZE 4096

// COM口的名称长度
#define COM_NAME_SIZE 16

// 符号名称长度
#define SYMBOL_NAME_SIZE 64

// 初始化unicode字符串
#define RTL_INIT_UNICODE(_obj, _length, _buff, _max_size) \
            {   \
                _obj.Length = _length;\
                _obj.Buffer = _buff; \
                _obj.MaximumLength = _max_size;\
            }
#pragma endregion