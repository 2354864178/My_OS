#ifndef ONIX_GLOBAL_H
#define ONIX_GLOBAL_H

#include <onix/types.h>

#define GDT_SIZE 128

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

void gdt_init();

#endif