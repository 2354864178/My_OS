
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/time.h>
#include <onix/rtc.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/devicetree.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static rtc_dt_info_t rtc_dt;

// 解析设备树中的 CMOS RTC 信息，仅用于与硬编码端口/IRQ 对比
static void rtc_dt_probe(void)
{
    void *val; u32 len;
    const char *paths[] = {"/rtc@70"};

    if (dtb_get_prop_any(paths, 1, "reg", &val, &len) == 0 && len >= 8)
    {
        u32 *cells = (u32 *)val;
        rtc_dt.addr_port = dt_be32_read(&cells[0]);
        if (len >= 16)
            rtc_dt.data_port = dt_be32_read(&cells[2]);
        rtc_dt.present = true;
        LOGK("DT rtc: addr 0x%x (code 0x%x), data 0x%x (code 0x%x)\n",
             rtc_dt.addr_port, CMOS_ADDR_PORT, rtc_dt.data_port, CMOS_DATA_PORT);
    }

    if (dtb_get_prop_any(paths, 1, "interrupts", &val, &len) == 0 && len >= 4)
    {
        rtc_dt.irq = dt_be32_read(val);
        rtc_dt.present = true;
        LOGK("DT rtc: irq %u (code %u)\n\n", rtc_dt.irq, IRQ_RTC);
    }
}

// 读 cmos 寄存器的值
u8 cmos_read(u8 addr)
{
    outb(rtc_dt.addr_port, CMOS_NMI_MASK | addr);
    return inb(rtc_dt.data_port);
};

// 写 cmos 寄存器的值
void cmos_write(u8 addr, u8 value)
{
    outb(rtc_dt.addr_port, CMOS_NMI_MASK | addr);
    outb(rtc_dt.data_port, value);
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

    cmos_write(CMOS_REG_HOURS_ALARM, bin_to_bcd(time.tm_hour));    // 写入目标小时（BCD格式）
    cmos_write(CMOS_REG_MINUTES_ALARM, bin_to_bcd(time.tm_min));   // 写入目标分钟（BCD格式）
    cmos_write(CMOS_REG_SECONDS_ALARM, bin_to_bcd(time.tm_sec));   // 写入目标秒（BCD格式）

    cmos_write(CMOS_REG_B, 0b00100010); // 打开闹钟中断
    cmos_read(CMOS_REG_C);              // 读 C 寄存器，以允许 CMOS 中断
}

void rtc_init()
{
    assert(dtb_node_enabled("/rtc@70"));
    rtc_dt_probe();
    set_interrupt_handler(IRQ_RTC, rtc_handler);
    set_interrupt_mask(IRQ_RTC, true);
    // set_interrupt_mask(IRQ_CASCADE, true);
}

// 供其他模块读取设备树探测到的 CMOS/RTC 信息
const rtc_dt_info_t *rtc_dt_get(void)
{
    return &rtc_dt;
}
