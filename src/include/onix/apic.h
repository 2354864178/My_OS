#ifndef ONIX_APIC_H
#define ONIX_APIC_H

#include <onix/types.h>
#include <onix/interrupt.h>

#define LAPIC_BASE_PHYS  0xFEE00000    // Local APIC 物理基址
#define IOAPIC_BASE_PHYS 0xFEC00000    // I/O APIC 物理基址

#define APIC_IRQ_BASE_VECTOR 0x20                                       // APIC 中断起始向量号
#define APIC_IRQ_TO_VECTOR(irq) ((u32)(APIC_IRQ_BASE_VECTOR + (irq)))   // IRQ 转向量号

// Local APIC 的“伪中断”向量号（用于开启 APIC/处理伪中断）
#define APIC_SPURIOUS_VECTOR 0x2Fu      // 47

// Local APIC 寄存器偏移（MMIO，单位：字节）
#define LAPIC_REG_ID   0x020u // APIC ID 寄存器
#define LAPIC_REG_EOI  0x0B0u // EOI（End Of Interrupt）寄存器：中断结束确认
#define LAPIC_REG_SVR  0x0F0u // SVR（Spurious Vector）寄存器：伪中断向量/使能位
#define LAPIC_REG_TPR  0x080u // TPR（Task Priority）寄存器：任务优先级

// Local APIC 定时器（用来替换 PIT 时钟）
#define LAPIC_REG_LVT_TIMER      0x320u // LVT Timer：本地向量表-定时器项
#define LAPIC_REG_TIMER_INITCNT  0x380u // 初始计数值
#define LAPIC_REG_TIMER_CURRCNT  0x390u // 当前计数值
#define LAPIC_REG_TIMER_DIV      0x3E0u // 分频配置

#define LAPIC_SVR_ENABLE (1u << 8) // SVR 第 8 位：软件使能 APIC

#define IOAPIC_REGSEL 0x00u // IOREGSEL：寄存器选择
#define IOAPIC_WINDOW 0x10u // IOWIN：寄存器窗口（读/写数据）

// I/O APIC 内部寄存器索引（写入 IOREGSEL 的值）
#define IOAPIC_REG_ID  0x00u    // I/O APIC ID 寄存器
#define IOAPIC_REG_VER 0x01u    // I/O APIC 版本寄存器
#define IOAPIC_REG_ARB 0x02u    // I/O APIC 仲裁 ID 寄存器

// 重定向表（Redirection Table）基础索引：
// IRQ n 的 low/high 对应内部寄存器：base + 2*n / base + 2*n + 1
#define IOAPIC_REDTBL_BASE 0x10u

// 重定向表项（Redirection Entry）的位域（64-bit）
#define IOAPIC_REDIR_VECTOR_MASK 0xFFull

#define IOAPIC_REDIR_DELIVMODE_SHIFT 8  // 传递模式字段起始位
#define IOAPIC_REDIR_DESTMODE_SHIFT  11 // 目的地模式字段起始位
#define IOAPIC_REDIR_POLARITY_SHIFT  13 // 极性字段起始位
#define IOAPIC_REDIR_TRIGGER_SHIFT   15 // 触发模式字段起始位
#define IOAPIC_REDIR_MASK_SHIFT      16 // 屏蔽字段起始位
#define IOAPIC_REDIR_DELIV_FIXED   (0ull << IOAPIC_REDIR_DELIVMODE_SHIFT)   // 固定模式
#define IOAPIC_REDIR_DEST_PHYSICAL (0ull << IOAPIC_REDIR_DESTMODE_SHIFT)    // 物理模式

#define IOAPIC_REDIR_POLARITY_HIGH (0ull << IOAPIC_REDIR_POLARITY_SHIFT)    // 高电平有效
#define IOAPIC_REDIR_POLARITY_LOW  (1ull << IOAPIC_REDIR_POLARITY_SHIFT)    // 低电平有效

#define IOAPIC_REDIR_TRIGGER_EDGE  (0ull << IOAPIC_REDIR_TRIGGER_SHIFT) // 边沿触发
#define IOAPIC_REDIR_TRIGGER_LEVEL (1ull << IOAPIC_REDIR_TRIGGER_SHIFT) // 电平触发

#define IOAPIC_REDIR_MASKED   (1ull << IOAPIC_REDIR_MASK_SHIFT)     // 屏蔽
#define IOAPIC_REDIR_UNMASKED (0ull << IOAPIC_REDIR_MASK_SHIFT)     // 取消屏蔽

// 目的地 APIC ID 放在 bits 56..63（常用 physical 模式，直接写 APIC ID）
#define IOAPIC_REDIR_DEST_SHIFT 56  // 目的地字段起始位
#define IOAPIC_REDIR_DEST(apic_id) (((u64)(apic_id) & 0xFFull) << IOAPIC_REDIR_DEST_SHIFT)  // 设置目的地 APIC ID

#endif