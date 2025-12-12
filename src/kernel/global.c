#include <onix/global.h>
#include <onix/string.h>
#include <onix/debug.h>

descriptor_t gdt[GDT_SIZE]; // 内核全局描述符表
pointer_t gdt_ptr;          // 内核全局描述符表指针
tss_t tss;                  // 任务状态段

// 初始化描述符
void descriptor_init(descriptor_t *desc, u32 base, u32 limit){
    desc->base_low = base & 0xffffff;       // 基地址低24位
    desc->base_high = (base >> 24) & 0xff;  // 基地址高8位
    desc->limit_low = limit & 0xffff;       // 段界限低16位
    desc->limit_high = (limit >> 16) & 0xf; // 段界限高4位
}

// 初始化内核全局描述符表
void gdt_init(){
    DEBUGK("init gdt!!!\n");
    memset(gdt, 0, sizeof(gdt));    // 清空全局描述符表

    descriptor_t *desc;                 // 临时描述符指针
    desc = gdt + KERNEL_CODE_IDX;       // 代码段描述符
    descriptor_init(desc, 0, 0xfffff);  // 基地址0，段界限4GB
    desc->present = 1;                  // 存在位
    desc->DPL = 0;                      // 特权级0
    desc->segment = 1;                  // 代码或数据段
    desc->big = 1;                      // 32位段
    desc->granularity = 1;              // 4KB粒度
    desc->type = 0xA;                   // 可执行、可读

    desc = gdt + KERNEL_DATA_IDX;       // 数据段描述符
    descriptor_init(desc, 0, 0xfffff);  // 基地址0，段界限4GB
    desc->present = 1;                  // 存在位
    desc->DPL = 0;                      // 特权级0
    desc->segment = 1;                  // 代码或数据段 
    desc->big = 1;                      // 32位段
    desc->granularity = 1;              // 4KB粒度
    desc->type = 0x2;                   // 可写、不可执行

    desc = gdt + USER_CODE_IDX;         // 用户态代码段描述符
    descriptor_init(desc, 0, 0xfffff);  // 基地址0，段界限4GB
    desc->present = 1;                  // 存在位
    desc->DPL = 3;                      // 特权级3
    desc->segment = 1;                  // 代码或数据段
    desc->big = 1;                      // 32位段
    desc->granularity = 1;              // 4KB粒度
    desc->type = 0xA;                   // 可执行、可读

    desc = gdt + USER_DATA_IDX;         // 用户态数据段描述符
    descriptor_init(desc, 0, 0xfffff);  // 基地址0，段界限4GB
    desc->present = 1;                  // 存在位
    desc->DPL = 3;                      // 特权级3
    desc->segment = 1;                  // 代码或数据段 
    desc->big = 1;                      // 32位段
    desc->granularity = 1;              // 4KB粒度
    desc->type = 0x2;                   // 可写、不可执行

    gdt_ptr.limit = sizeof(gdt) - 1;    // 全局描述符表大小
    gdt_ptr.base = (u32)&gdt;           // 全局描述符表地址
}

// 初始化任务状态段
void tss_init(){
    memset(&tss, 0, sizeof(tss));

    tss.ss0 = KERNEL_DATA_SELECTOR;   // 内核数据段的选择子
    tss.iobase = sizeof(tss);  // iobase的存在是为了用户态也可以直接做IO，目前我们用不到

    descriptor_t *desc = gdt + KERNEL_TSS_IDX;
    descriptor_init(desc, (u32)&tss, sizeof(tss) - 1);
    desc->segment = 0;     // 系统段
    desc->granularity = 0; // 字节
    desc->big = 0;         // 固定为 0
    desc->long_mode = 0;   // 固定为 0
    desc->present = 1;     // 在内存中
    desc->DPL = 0;         // 用于任务门或调用门
    desc->type = 0b1001;   // 32 位可用 tss  (1011表示 32位TSS忙)

    BMB;
    asm volatile(
        "ltr %%ax\n" ::"a"(KERNEL_TSS_SELECTOR));
}
