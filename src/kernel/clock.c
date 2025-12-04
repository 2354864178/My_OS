#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/task.h>
// #include <onix/timer.h>

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

void clock_handler(int vector)
{
    assert(vector == 0x20); // 时钟中断向量号 0x20

    send_eoi(vector);   // 发送中断处理结束
    stop_beep();        // 停止蜂鸣器

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
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}
