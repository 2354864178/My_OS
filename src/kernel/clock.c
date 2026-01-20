#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/task.h>
#include <onix/devicetree.h>
// #include <onix/timer.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define PIT_CHAN0_REG 0X40  // 计数器 0 端口
#define PIT_CHAN2_REG 0X42  // 用于蜂鸣器
#define PIT_CTRL_REG 0X43   // 8253/8254 控制寄存器

#define HZ 100              // 时钟中断频率
#define OSCILLATOR 1193182  // 8253/8254 时钟芯片的输入频率
#define CLOCK_COUNTER (OSCILLATOR / HZ) // 计数器初值
#define JIFFY (1000 / HZ)   // 每个时钟节拍的毫秒数

#define SPEAKER_REG 0x61    // 蜂鸣器端口
#define BEEP_HZ 440         // 蜂鸣器频率
#define BEEP_COUNTER (OSCILLATOR / BEEP_HZ) // 蜂鸣器计数器初值
// #define BEEP_MS 100

u32 volatile jiffies = 0;   // 全局时钟节拍计数
u32 jiffy = JIFFY;          // 每个时钟节拍的毫秒数

bool volatile beeping = 0;

typedef struct pit_dt_info
{
    bool present;   // 信息是否有效
    u32 chan0;      // 计数器 0 端口
    u32 chan2;      // 计数器 2 端口
    u32 ctrl;       // 控制端口
    u32 irq;        // 中断号
    u32 clock_hz;   // 时钟频率
} pit_dt_info_t;

static pit_dt_info_t pit_dt;

// 解析设备树中的 PIT 信息，仅用于与硬编码端口/IRQ 对比
static void pit_dt_probe(void)
{
    void *val; u32 len;
    const char *paths[] = {"/timer@40"};

    if (dtb_get_prop_any(paths, 1, "reg", &val, &len) == 0 && len >= 8)
    {
        u32 *cells = (u32 *)val;
        // reg 属性按 <addr size> 成对排列，至少会给出 chan0；尝试读取 chan2、ctrl
        pit_dt.chan0 = dt_be32_read(&cells[0]);
        if (len >= 16)
            pit_dt.chan2 = dt_be32_read(&cells[2]);
        if (len >= 24)
            pit_dt.ctrl = dt_be32_read(&cells[4]);
        pit_dt.present = true;
        LOGK("DT pit: chan0 0x%x (code 0x%x), chan2 0x%x (code 0x%x), ctrl 0x%x (code 0x%x)\n",
             pit_dt.chan0, PIT_CHAN0_REG, pit_dt.chan2, PIT_CHAN2_REG, pit_dt.ctrl, PIT_CTRL_REG);
    }

    if (dtb_get_prop_any(paths, 1, "interrupts", &val, &len) == 0 && len >= 4)
    {
        pit_dt.irq = dt_be32_read(val);
        pit_dt.present = true;
        LOGK("DT pit: irq %u (code %u)\n",
             pit_dt.irq, IRQ_CLOCK);
    }

    if (dtb_get_prop_any(paths, 1, "clock-frequency", &val, &len) == 0 && len >= 4)
    {
        pit_dt.clock_hz = dt_be32_read(val);
        pit_dt.present = true;
        LOGK("DT pit: clock %u Hz (code %u Hz)\n\n",
             pit_dt.clock_hz, OSCILLATOR);
    }
}

void start_beep()
{
    if (!beeping) outb(SPEAKER_REG, inb(SPEAKER_REG) | 3);  // 打开蜂鸣器 

    DEBUGK("PC speaker BB\n", jiffies);
    beeping = jiffies + 5;
}

void stop_beep() {
    if (beeping &&  jiffies >beeping) {
        outb(SPEAKER_REG, inb(SPEAKER_REG) & 0xfc); // 关闭蜂鸣器
        beeping = 0;    // 重置标志
    }
}

extern void task_wakeup();

void clock_handler(int vector)
{
    assert(vector == 0x20); // 时钟中断向量号 0x20

    send_eoi(vector);   // 发送中断处理结束
    stop_beep();        // 停止蜂鸣器

    task_wakeup();      // 唤醒睡眠任务

    jiffies++;          // 全局时钟节拍计数加一

    task_t *task = running_task();      // 获取当前运行任务指针
    // printk("Clock tick: %d\n", task->magic);
    assert(task->magic == ONIX_MAGIC);  // 检查任务魔数，防止栈溢出

    task->jiffies = jiffies;            // 更新任务的 jiffies 字段
    task->ticks--;                      // 当前任务时间片减一

    if (!task->ticks) {                 // 若时间片用完
        task->ticks = task->priority;   // 重置时间片为优先级值
        schedule();                     // 进行任务调度
    }
}

extern u32 startup_time;                // 系统启动时间，单位毫秒
time_t sys_time(){
    return startup_time + jiffies * JIFFY / 1000;   // 返回系统运行时间，单位秒
}

void pit_init()
{
    // 配置计数器 0 时钟
    outb(PIT_CTRL_REG, 0b00110100);                     // 方式 2, 16 位二进制, 读写低高字节
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);          // 计数器低 8 位
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);   // 计数器高 8 位

    // 配置计数器 2 蜂鸣器
    outb(PIT_CTRL_REG, 0b10110110);                     // 方式 3, 16 位二进制, 读写低高字节
    outb(PIT_CHAN2_REG, (u8)BEEP_COUNTER);              // 计数器低 8 位
    outb(PIT_CHAN2_REG, (u8)(BEEP_COUNTER >> 8));       // 计数器高 8 位
}

void clock_init(){
    assert(dtb_node_enabled("/timer@40"));
    pit_dt_probe();
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}
