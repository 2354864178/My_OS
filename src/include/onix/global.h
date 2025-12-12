#ifndef ONIX_GLOBAL_H
#define ONIX_GLOBAL_H

#include <onix/types.h>

#define GDT_SIZE 128   

#define KERNEL_CODE_IDX 1   // 代码段选择子索引
#define KERNEL_DATA_IDX 2   // 数据段选择子索引
#define KERNEL_TSS_IDX 3    // TSS 段选择子索引

#define USER_CODE_IDX 4
#define USER_DATA_IDX 5

#define KERNEL_CODE_SELECTOR (KERNEL_CODE_IDX << 3) // 选择子 = 索引 << 3
#define KERNEL_DATA_SELECTOR (KERNEL_DATA_IDX << 3) // 选择子 = 索引 << 3
#define KERNEL_TSS_SELECTOR (KERNEL_TSS_IDX << 3)   // 选择子 = 索引 << 3

#define USER_CODE_SELECTOR (USER_CODE_IDX << 3 | 0b11)  // RPL = 3
#define USER_DATA_SELECTOR (USER_DATA_IDX << 3 | 0b11)  // RPL = 3

// 全局描述符
// unsigned char 本身是 8 位的整数类型（1 字节），但在结构体中，它可以作为 “容器”，通过位域语法（成员名 : 位数）将这 8 位分割成多个独立的 “子成员”，每个子成员占用指定的位数。
// #pragma pack 的对齐控制 优先级高于编译器默认规则和大多数其他对齐属性，用#define _packed __attribute__((_packed))有时候不起作用，不知道为什么
#pragma pack(1) 
typedef struct descriptor_t /* 共 8 个字节 */
{
    unsigned short limit_low;      // 段界限 0 ~ 15 位
    unsigned int base_low : 24;    // 基地址 0 ~ 23 位 16M
    unsigned char type : 4;        // 段类型
    unsigned char segment : 1;     // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;     // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;  // 段界限 16 ~ 19;
    unsigned char available : 1;   // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;   // 64 位扩展标志
    unsigned char big : 1;         // 32 位 还是 16 位;
    unsigned char granularity : 1; // 粒度 4KB 或 1B
    unsigned char base_high;       // 基地址 24 ~ 31 位
} descriptor_t;
#pragma pack() 

// 段选择子
typedef struct selector_t
{
    u8 RPL : 2; // Request Privilege Level
    u8 TI : 1;  // Table Indicator
    u16 index : 13; 
} selector_t;

// 全局描述符表指针
#pragma pack(1)  
typedef struct pointer_t
{
    u16 limit;
    u32 base;
}pointer_t;    // 取消内存对齐，强制紧密排列
#pragma pack()  

typedef struct tss_t
{
    u32 backlink; // 前一个任务的链接，保存了前一个任状态段的段选择子
    u32 esp0;     // ring0 的栈顶地址
    u32 ss0;      // ring0 的栈段选择子
    u32 esp1;     // ring1 的栈顶地址
    u32 ss1;      // ring1 的栈段选择子
    u32 esp2;     // ring2 的栈顶地址
    u32 ss2;      // ring2 的栈段选择子
    u32 cr3;      
    u32 eip;
    u32 flags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldtr;          // 局部描述符选择子
    u16 trace : 1;     // 如果置位，任务切换时将引发一个调试异常
    u16 reversed : 15; // 保留不用
    u16 iobase;        // I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移
    u32 ssp;           // 任务影子栈指针
} _packed tss_t;

void gdt_init();

#endif