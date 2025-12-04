#ifndef ONIX_TASK_H
#define ONIX_TASK_H

#include <onix/types.h>

#define KERNEL_USER 0
#define NORMAL_USER 1

#define TASK_NAME_LEN 16

typedef u32 target_t(); // 任务入口函数类型

typedef enum task_state_t
{
    TASK_INIT,     // 初始化
    TASK_RUNNING,  // 执行
    TASK_READY,    // 就绪
    TASK_BLOCKED,  // 阻塞
    TASK_SLEEPING, // 睡眠
    TASK_WAITING,  // 等待
    TASK_DIED,     // 死亡
} task_state_t;

typedef struct task_t
{
    u32 *stack;                 // 内核栈
    task_state_t state;         // 任务状态
    u32 priority;               // 任务优先级
    int ticks;                  // 剩余时间片
    u32 jiffies;                // 上次执行时全局时间片
    char name[TASK_NAME_LEN];   // 任务名
    u32 uid;                    // 用户 id
    u32 pde;                    // 页目录物理地址
    struct bitmap_t *vmap;      // 进程虚拟内存位图
    u32 magic;                  // 内核魔数，用于检测栈溢出
} task_t;

typedef struct task_frame_t{
    u32 edi;    // 保存的 edi 寄存器值
    u32 esi;    // 保存的 esi 寄存器值
    u32 ebx;
    u32 ebp;
    void (*eip)(void);  // 返回地址
} task_frame_t;

task_t *running_task(); // 获取当前运行的任务指针
void schedule();

void task_yield();
#endif