#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/bitmap.h>
#include <onix/assert.h>
#include <onix/interrupt.h>
#include <onix/memory.h>
#include <onix/string.h>

extern bitmap_t kernel_map;             // 内核内存位图
extern void task_switch(task_t *next);  // 任务切换汇编函数

#define TASK_NR 64                  // 最大任务数
static task_t *task_table[TASK_NR]; // 任务表

// 返回一个空闲任务结构的指针
task_t *get_free_task() { 
    for (int i = 0; i < TASK_NR; i++) // 遍历任务表寻找空闲槽，i 为索引
    { 
        if (task_table[i] == NULL) { 
            task_table[i] = (task_t *)alloc_kpage(1);   // 为任务分配一页内核页并存入表中
            return task_table[i];                       // 返回新分配的任务指针
        } 
    } 
    panic("No Free Task!!!"); // 若无空闲任务则触发 panic
} 

// 根据任务状态搜索任务，返回第一个匹配状态的任务指针，若无匹配则返回 NULL
task_t *task_search(task_state_t state){
    assert(!get_interrupt_state());     // 禁止中断时调用

    task_t *task=NULL;                  // 初始化任务指针为空
    task_t *current = running_task();   // 获取当前运行任务指针

    for (int i = 0; i < TASK_NR; i++) {
        task_t *t = task_table[i];          // 获取任务表中的任务指针

        if(t == NULL) continue;            // 跳过空槽
        if(t->state != state) continue;    // 跳过状态不匹配的任务
        if(t == current) continue;         // 跳过当前任务

        if (task == NULL || task->ticks < t->ticks || t->jiffies < task->jiffies)
            task = t;;                      // 选择剩余时间片更多或上次执行时间更早的任务
        // if (t->state == state) return t;    // 找到匹配状态的任务则返回其指针
    }
    return task;
}

task_t *running_task(){                 // 获取当前运行的任务指针
    asm volatile(
        "movl %esp, %eax\n"             // 将当前栈指针 esp 的值存入 eax 寄存器
        "andl $0xfffff000, %eax\n"      // 将 eax 寄存器的值与 0xfffff000 进行按位与运算，得到当前任务结构体的基地址
    );
}

// 任务调度函数
void schedule(){
    assert(!get_interrupt_state());         // 确保在不可被中断的上下文中调用

    task_t *current = running_task();       // 获取当前运行的任务指针
    task_t *next = task_search(TASK_READY); // 从就绪队列选择下一个任务

    assert(next != NULL);                   // 确保找到了可运行的任务
    assert(next->magic == ONIX_MAGIC);      // 校验任务结构的魔数以检测损坏

    if (current->state == TASK_RUNNING) { 
        current->state = TASK_READY;        // 如果当前仍然被标记为运行中，将当前任务标记为就绪
    } 

    next->state = TASK_RUNNING;     // 将选择的下一个任务标记为运行中
    if (next == current) return;    // 如果下一个任务就是当前任务，无需切换，直接返回
    task_switch(next);              // 执行上下文切换到下一个任务
}

// 创建一个新任务并初始化其任务结构体
static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid){
    task_t *task = get_free_task(); // 分配一个空闲任务结构体
    memset(task, 0, PAGE_SIZE);     // 清空任务结构体内存

    u32 stack = (u32)task + PAGE_SIZE;  // 计算任务栈顶（任务结构体起始地址 + 一页大小）
    stack -= sizeof(task_frame_t);      // 为任务帧留出空间，调整栈指针
    task_frame_t *frame = (task_frame_t *)stack; // 将栈指针转换成任务帧指针

    frame->ebx = 0x11111111; // 初始化保存的 ebx 寄存器值（占位）
    frame->esi = 0x22222222; // 初始化保存的 esi 寄存器值（占位）
    frame->edi = 0x33333333; // 初始化保存的 edi 寄存器值（占位）
    frame->ebp = 0x44444444; // 初始化保存的 ebp 寄存器值（占位）
    frame->eip = (void *)target;    // 设置任务的入口点为目标函数地址   

    strcpy((char *)task->name, name);   // 复制任务名称到任务结构体

    task->stack = (u32 *)stack;                 // 设置任务的栈指针
    task->priority = priority ? priority : 1;   // 设置优先级，若为0则使用默认值1
    task->ticks = task->priority;               // 初始时间片等于优先级
    task->jiffies = 0;                          // 初始化任务运行计时器为0
    task->state = TASK_READY;                   // 将任务状态设置为就绪
    task->uid = uid;                            // 设置任务所属用户ID
    task->vmap = &kernel_map;                   // 设置任务使用的虚拟内存位图为内核内存位图
    task->pde = KERNEL_PAGE_DIR;                // 设置任务的页目录地址为内核页目录地址
    task->magic = ONIX_MAGIC;                   // 设置魔数以便后续校验结构完整性

    return task;
}

// 初始化任务系统
static void task_setup(){
    task_t *task = running_task();  // 获取当前运行任务指针
    task->magic = ONIX_MAGIC;       // 设置魔数以便后续校验结构完整性
    task->ticks = 1;                // 初始化时间片为1

    memset(task_table, 0, sizeof(task_table)); // 清空任务表
}

u32 thread_a(){
    set_interrupt_state(true);
    while(true){
        printk("Thread A running...\n");
    }
}

u32 thread_b(){
    set_interrupt_state(true);
    while(true){
        printk("Thread B running...\n");
    }
}

u32 thread_c(){
    set_interrupt_state(true);
    while(true){
        printk("Thread C running...\n");
    }
}

void task_init(){
    task_setup();  // 初始化任务系统

    task_create(thread_a, "thread_a", 5, KERNEL_USER); // 创建线程 A，优先级 3
    task_create(thread_b, "thread_b", 5, KERNEL_USER); // 创建线程 B，优先级 5
    task_create(thread_c, "thread_c", 5, KERNEL_USER); // 创建线程 C，优先级 8

    printk("Task init done!\n");
}
