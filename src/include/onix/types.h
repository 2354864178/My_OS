#ifndef ONIX_TYPES_H    // 头文件保护宏：防止该头文件被重复包含（避免宏定义冲突、类型重定义错误）
#define ONIX_TYPES_H

#include <onix/onix.h>

#define EOF -1              // End Of File：文件结束符，用于文件系统、字符设备读取（如串口、硬盘）时表示读取完毕
#define EOS '\0'            // End Of String：字符串结束符，内核字符串处理（如strlen、strcpy）的终止标志
#define NULL ((void *)0)    // 空指针常量：初始化指针变量（避免野指针），用于判断指针有效性（如if (ptr == NULL)）

#define CONCAT(x, y) x##y
#define RESERVED_TOKEN(x, y) CONCAT(x, y)
#define RESERVED RESERVED_TOKEN(reserved, __LINE__)

#ifndef __cplusplus // 仅在C语言编译环境下生效（C++有原生bool，避免冲突）
#define bool _Bool  // 基于C99标准的_Bool类型，定义内核统一的bool类型
#define true 1
#define false 0
#endif

#define _packed __attribute__((packed))                         // 用于定义特殊的结构体(取消内存对齐)
#define _ofp __attribute__((optimize("omit-frame-pointer")))    // 用于省略函数的栈帧
#define _inline __attribute__((always_inline)) inline           // 同时触发标准 C 的内联建议和GCC 的强制内联要求，确保编译器必须把函数内联到调用处
                                                                // 针对内核中短小、高频、性能敏感的函数，消除函数调用开销；

// 有符号整数类型（明确位宽，适配硬件操作和数值计算）
typedef unsigned int size_t;
typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

// 无符号整数类型（核心用于硬件操作、位运算、地址表示）
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef int32 pid_t;    // 进程ID类型：定义为32位有符号整数，表示系统中唯一标识一个进程的ID
typedef int32 dev_t;    // 设备号类型：定义为32位有符号整数，表示系统中唯一标识一个设备的编号

typedef u32 time_t;     // 时间戳类型：定义为32位无符号整数，存储"从1970-01-01 00:00:00到当前的秒数"
typedef u32 idx_t;

typedef int32 fd_t;     // 文件描述符类型：定义为32位有符号整数，表示打开的文件、设备等资源的索引
typedef enum std_fd_t{  // 标准文件描述符枚举类型
    stdin,      // 标准输入，通常对应键盘输入
    stdout,     // 标准输出，通常对应屏幕显示
    stderr,     // 标准错误输出，通常对应错误信息显示
} std_fd_t;

#endif      // 头文件保护宏结束：与开头ONIX_TYPES_H对应，标记头文件内容结束