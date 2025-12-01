
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/time.h>
#include <onix/assert.h>
#include <onix/stdlib.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define CMOS_ADDR 0x70 // CMOS 地址寄存器
#define CMOS_DATA 0x71 // CMOS 数据寄存器

#define CMOS_SECOND 0x01    // 秒
#define CMOS_MINUTE 0x03    // 分
#define CMOS_HOUR 0x05      // 时

#define CMOS_A 0x0a     // 寄存器 A
#define CMOS_B 0x0b     // 寄存器 B
#define CMOS_C 0x0c
#define CMOS_D 0x0d
#define CMOS_NMI 0x80   // 非屏蔽中断位

// 读 cmos 寄存器的值
u8 cmos_read(u8 addr)
{
    outb(CMOS_ADDR, CMOS_NMI | addr);
    return inb(CMOS_DATA);
};

// 写 cmos 寄存器的值
void cmos_write(u8 addr, u8 value)
{
    outb(CMOS_ADDR, CMOS_NMI | addr);
    outb(CMOS_DATA, value);
}

extern void start_beep();

static u32 volatile counter=0;
// 实时时钟中断处理函数
void rtc_handler(int vector)
{
    
    assert(vector == 0x28); // 实时时钟中断向量号
    send_eoi(vector);       // 向中断控制器发送中断处理完成的信号

    // 读 CMOS 寄存器 C，允许 CMOS 继续产生中断
    // cmos_read(CMOS_C);
    // LOGK("rtc handler %d...\n", counter++);
    start_beep();
}

// 设置 secs 秒后发生实时时钟中断
void set_alarm(u32 secs)
{
    LOGK("beeping after %d seconds\n", secs);   // 打印闹钟提示：告知用户将在secs秒后触发闹钟（调试/用户反馈用）

    tm time;
    time_read(&time);           // 读取当前系统时间（二进制格式，已由time_read完成BCD→二进制转换）

    u8 sec = secs % 60;     // 拆分出"额外秒数"（0~59）
    secs /= 60;             // 剩余秒数转换为分钟单位
    u8 min = secs % 60;     // 拆分出"额外分钟数"（0~59）
    secs /= 60;             // 剩余分钟数转换为小时单位
    u32 hour = secs;        // 拆分出"额外小时数"（无上限，后续会处理24小时循环）

    time.tm_sec += sec;
    if (time.tm_sec >= 60) {
        time.tm_sec %= 60;  // 秒数取模60，保留0~59范围
        time.tm_min += 1;   // 秒满60，向分钟进1
    }

    time.tm_min += min;
    if (time.tm_min >= 60) {
        time.tm_min %= 60;  // 分钟数取模60，保留0~59范围
        time.tm_hour += 1;  // 分满60，向小时进1
    }

    time.tm_hour += hour;
    if (time.tm_hour >= 24) {
        time.tm_hour %= 24; // 小时数取模24，保留0~23范围（符合24小时制）
    }

    cmos_write(CMOS_HOUR, bin_to_bcd(time.tm_hour));    // 写入目标小时（BCD格式）
    cmos_write(CMOS_MINUTE, bin_to_bcd(time.tm_min));   // 写入目标分钟（BCD格式）
    cmos_write(CMOS_SECOND, bin_to_bcd(time.tm_sec));   // 写入目标秒（BCD格式）

    cmos_write(CMOS_B, 0b00100010); // 打开闹钟中断
    cmos_read(CMOS_C);              // 读 C 寄存器，以允许 CMOS 中断
}

void rtc_init()
{
    // cmos_write(CMOS_B, 0b01000010); // 打开周期中断
    // cmos_read(CMOS_C);
    // // set_alarm(1);
    // // 设置中断频率
    // outb(CMOS_A, (inb(CMOS_A) & 0xf) | 0b1110);

    set_interrupt_handler(IRQ_RTC, rtc_handler);
    set_interrupt_mask(IRQ_RTC, true);
    set_interrupt_mask(IRQ_CASCADE, true);
}
