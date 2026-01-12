#include <onix/task.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/arena.h>
#include <onix/bitmap.h>
#include <onix/assert.h>
#include <onix/interrupt.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/list.h>
#include <onix/global.h>

#define TASK_NR 64                  // 最大任务数
#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 jiffy;                       // 每个时钟节拍的毫秒数
extern tss_t tss;                       // 任务状态段
extern u32 volatile jiffies;            // 全局时钟节拍计数
extern bitmap_t kernel_map;             // 内核内存位图
extern void interrupt_exit();           // 中断退出处理程序
extern void task_switch(task_t *next);  // 任务切换汇编函数

static list_t block_list;           // 任务阻塞链表
static list_t sleep_list;           // 任务睡眠链表
static task_t *idle_task;           // 空闲任务指针
static task_t *task_table[TASK_NR]; // 任务表

// 返回一个空闲任务结构的指针
task_t *get_free_task() { 
    // 遍历任务表寻找空闲槽，i 为索引
    for (int i = 0; i < TASK_NR; i++) { 
        if (task_table[i] == NULL) { 
            task_t *task = (task_t *)alloc_kpage(1);    // 为任务分配一页内核页并存入表中
            memset(task, 0, sizeof(task_t));            // 清空任务结构体
            task->pid = i;                              // 设置任务ID为索引值
            task_table[i] = task;                       // 存入任务表   
            return task_table[i];                       // 返回新分配的任务指针
        } 
    } 
    panic("No Free Task!!!"); // 若无空闲任务则触发 panic
} 

pid_t sys_getpid(){
    task_t *current = running_task();   // 获取当前运行任务指针
    return current->pid;                // 返回当前任务的进程ID
}

pid_t sys_getppid(){
    task_t *current = running_task();   // 获取当前运行任务指针
    return current->ppid;               // 返回当前任务的父进程ID
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
    }
    if(task == NULL && state == TASK_READY){
        task = idle_task;                   // 若无就绪任务则返回空闲任务
    }

    return task;
}

void task_yield(){ 
    schedule();    // 调用调度函数
}

void task_block(task_t *task, list_t *blist, task_state_t state){
    // task: 要阻塞的任务指针
    // blist: 任务阻塞链表指针，若为 NULL 则不加入任何链表
    // state: 任务阻塞后的状态

    assert(!get_interrupt_state());     // 禁止中断时调用
    assert(task->magic == ONIX_MAGIC);  // 校验任务结构的魔数以检测损坏
    assert(task->node.next == NULL && task->node.prev == NULL);    // 任务节点不应在任何链表中
    
    if(blist == NULL){
        blist =  &block_list;   // 使用全局阻塞链表
    }
    list_push(blist, &task->node);      // 将任务节点加入阻塞链表

    assert(state != TASK_RUNNING && state != TASK_READY); // 阻塞状态不能为运行或就绪状态

    task->state = state;                // 设置任务状态为指定状态

    task_t *current = running_task();   // 获取当前运行任务指针
    if(current == task) schedule();     // 若阻塞当前任务则进行调度
}

void task_unlock(task_t *task){
    // task: 要解锁的任务指针
    assert(!get_interrupt_state());     // 禁止中断时调用
    assert(task->magic == ONIX_MAGIC);  // 校验任务结构的魔数以检测损坏

    list_remove(&task->node);           // 从阻塞链表中移除任务节点

    assert(task->node.next == NULL && task->node.prev == NULL);    // 确保任务节点已从链表中移除

    task->state = TASK_READY;           // 设置任务状态为就绪
}

void task_sleep(u32 ms){
    // ms: 睡眠时间，单位毫秒
    assert(!get_interrupt_state());     // 禁止中断时调用

    u32 ticks = ms / jiffy;                 // 计算需要的时钟节拍数
    ticks = ticks ? ticks : 1;              // 最少睡眠一个时钟节拍

    task_t *current = running_task();       // 获取当前运行任务指针
    current->ticks = jiffies+ticks;              // 更新任务的 jiffies 字段

    list_t *list = &sleep_list;             // 使用全局睡眠链表
    list_node_t *anchor = &list->tail;   // 获取当前任务的链表节点
    
    // 在睡眠链表中找到合适的插入位置，保持链表按唤醒时间排序
    for(list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next){
        task_t *task = element_entry(task_t, node, ptr); // 获取任务指针

        if(current->ticks < task->ticks){
            anchor = ptr;    // 找到插入位置的前一个节点
            break;
        }
    }
    list_insert_before(anchor, &current->node); // 插入任务节点到睡眠链表
    current->state = TASK_SLEEPING;             // 设置任务状态为睡眠
    schedule();                                 // 进行任务调度
}

void task_wakeup(){
    // 唤醒睡眠时间到期的任务
    assert(!get_interrupt_state());     // 禁止中断时调用

    list_t *list = &sleep_list;         // 使用全局睡眠链表
    list_node_t *ptr = list->head.next; // 从链表头开始遍历
    while(ptr != &list->tail){
        task_t *task = element_entry(task_t, node, ptr);    // 获取任务指针
        if(task->ticks > jiffies) break;                    // 若任务的唤醒时间未到则停止遍历

        ptr = ptr->next; ;  // 继续处理下一个节点
        task->ticks = 0;    // 重置任务的 ticks 字段
        task_unlock(task);  // 解锁任务
    }
}

// 激活任务
void task_activate(task_t *task){
    assert(task->magic == ONIX_MAGIC);
    if(task->pde != get_cr3()){
        set_cr3(task->pde);                 // 切换到任务的页目录
    }
    if (task->uid != KERNEL_USER){
        tss.esp0 = (u32)task + PAGE_SIZE;   // 获取用户进程的内核栈，因为只有用户进行做特权级切换会用到，内核进程时用不到的
    }
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

    if(!current->ticks){ 
        current->ticks = current->priority; // 如果当前任务的时间片用完，重置时间片为其优先级值
    }

    next->state = TASK_RUNNING;     // 将选择的下一个任务标记为运行中
    if (next == current) return;    // 如果下一个任务就是当前任务，无需切换，直接返回
    task_activate(next);            // 激活下一个任务的内存空间等资源
    task_switch(next);              // 执行上下文切换到下一个任务
}

// 创建一个新任务并初始化其任务结构体
static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid){
    // target: 任务入口函数指针
    // name: 任务名称
    // priority: 任务优先级
    // uid: 任务所属用户 ID

    task_t *task = get_free_task(); // 分配一个空闲任务结构体

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
    task->brk = KERNEL_MEMORY_SIZE;             // 初始化进程堆内存最高地址
    task->magic = ONIX_MAGIC;                   // 设置魔数以便后续校验结构完整性

    return task;
}

// 构建子任务的栈帧
static void task_build_stack(task_t *task){
    u32 addr = (u32)task + PAGE_SIZE;   // 计算任务栈顶地址
    addr -= sizeof(intr_frame_t);       // 为中断帧留出空间
    intr_frame_t *iframe = (intr_frame_t *)(addr);   // 在栈顶创建中断帧
    iframe->eax = 0;                    // 初始化 eax 寄存器值为0 ，也就是子进程的返回值为0
    addr -= sizeof(task_frame_t);       // 为任务帧留出空间
    task_frame_t *frame = (task_frame_t *)addr; // 在中断帧下方创建任务帧
    frame->ebp = 0xaa55aa55;            // 初始化保存的 ebp 寄存器值
    frame->ebx = 0xaa55aa55;            // 初始化保存的 ebx 寄存器值
    frame->edi = 0xaa55aa55;
    frame->esi = 0xaa55aa55;
    frame->eip = interrupt_exit;        // 设置任务的返回地址为中断退出处理程序
    task->stack = (u32 *)frame;         // 设置任务的栈指针
}

pid_t task_fork(){
    task_t *parent = running_task();
    assert(parent->node.next == NULL && parent->node.prev == NULL);
    assert(parent->state == TASK_RUNNING);

    // 先在临界区外分配会睡眠的资源
    void *child_page = (void *)alloc_kpage(1);          // 分配一页内核页作为子任务的任务结构体
    if(!child_page) panic("alloc child page failed");   // 分配失败则触发 panic

    bitmap_t *vmap = kmalloc(sizeof(bitmap_t));         // 分配子任务的虚拟内存位图结构体
    if(!vmap) panic("kmalloc vmap failed");             
    void *vmap_bits = (void *)alloc_kpage(1);           // 分配一页内核页作为子任务的虚拟内存位图缓冲区
    if(!vmap_bits) panic("alloc vmap bits failed"); 

    u32 child_pde = copy_pde();                         // 复制页目录（可能会睡眠），提前完成

    // 复制位图内容（使用父的 vmap->bits）
    memcpy(vmap, parent->vmap, sizeof(bitmap_t));       // 复制父任务的虚拟内存位图结构体内容
    memcpy(vmap_bits, parent->vmap->bits, PAGE_SIZE);   // 复制父任务的虚拟内存位图缓冲区内容
    vmap->bits = vmap_bits;                             // 设置子任务的虚拟内存位图缓冲区指针

    // 在短临界区内把 child 插入任务表并初始化（避免在临界区内分配/睡眠）
    bool intr = interrupt_disable();

    int slot = -1;
    for (int i = 0; i < TASK_NR; i++){
        if (task_table[i] == NULL){
            slot = i;
            break;
        }
    }
    if (slot == -1){    // 没有可用槽，回收已分配资源并报错
        set_interrupt_state(intr);
        free_kpage((u32)child_page, 1);
        free_kpage((u32)vmap_bits, 1);
        kfree(vmap);
        panic("No Free Task!!!");
    }

    task_t *child = (task_t *)child_page;
    task_table[slot] = child;           // 注册到任务表

    memcpy(child, parent, PAGE_SIZE);   // 复制父任务的整个 page 到子任务页（保留栈快照）

    // 修正子任务元数据 
    child->pid = slot;              // 设置子任务的进程ID
    child->ppid = parent->pid;      // 设置子任务的父进程ID
    child->state = TASK_READY;      // 设置子任务状态为就绪
    child->ticks = child->priority; // 重置子任务的时间片

    child->vmap = vmap;         // 设置子任务的虚拟内存位图指针
    child->pde = child_pde;     // 设置子任务的页目录地址

    task_build_stack(child);    // 构建子任务的栈帧

    set_interrupt_state(intr);

    return child->pid;
}

void task_exit(int status){
    task_t *task = running_task();
    assert(task->node.prev == NULL && task->node.next == NULL); // 任务不在任何阻塞队列中
    assert(task->state == TASK_RUNNING);        // 任务处于运行状态
    task->state = TASK_DIED;                    // 设置任务状态为死亡
    task->status = status;                      // 设置任务退出状态码
    free_pde();                                 // 释放任务的页目录和所有内存映射   
    free_kpage((u32)task->vmap->bits, 1);       // 释放任务的虚拟内存位图缓冲区
    kfree(task->vmap);                          // 释放任务的虚拟内存位图结构体
    for (size_t i = 0; i < TASK_NR; i++) {      
        task_t *child = task_table[i];
        if (!child) continue;
        if (child->ppid != task->pid) continue;
        child->ppid = task->ppid;               // 将子进程的父进程ID设置为当前任务的父进程ID
    }
    LOGK("Task %d exit with status %d\n", task->pid, status);

    task_t *parent = task_table[task->ppid]; // 获取父任务指针
    if(parent->state == TASK_WAITING && 
       (parent->waitpid == -1 || parent->waitpid == task->pid)){
        task_unlock(parent);    // 若父任务在等待当前任务则解锁父任务
    }

    schedule();                                 // 调度器切换到下一个任务
}

pid_t task_waitpid(pid_t pid, int *status){
    task_t *current = running_task();       // 获取当前运行任务指针
    task_t *child = NULL;                   // 初始化子任务指针为空

    while(true){
        int found = 0;                      // 标记是否找到指定的子进程
        for(size_t i = 0; i < TASK_NR; i++){
            task_t *child = task_table[i];
            if (!child) continue;
            if (child->ppid != current->pid) continue;      // 只检查当前任务的子进程
            if (pid != -1 && child->pid != pid) continue;   // 如果指定了 pid，则只检查该 pid 的子进程

            if (child->state == TASK_DIED) {
                task_table[i] = NULL;               // 从任务表中移除已终止的子进程
                *status = child->status;            // 获取子进程的退出状态码
                u32 ret = child->pid;               // 保存子进程的 PID                         
                free_kpage((u32)child, 1);          // 释放子进程的任务结构体内存
                return ret;                         // 返回已终止子进程的 PID
            }
            found = 1; // 找到符合条件的子进程
        }

        if (found) {
            current->waitpid = pid; // 设置当前任务的等待 PID
            task_block(current, NULL, TASK_WAITING); // 阻塞当前任务，等待子进程终止
            continue;
        }
        break; // 没有找到符合条件的子进程，退出循环
    }
    return -1; // 没有符合条件的子进程，返回 -1
}

// 初始化任务系统
static void task_setup(){
    task_t *task = running_task();  // 获取当前运行任务指针
    task->magic = ONIX_MAGIC;       // 设置魔数以便后续校验结构完整性
    task->ticks = 1;                // 初始化时间片为1

    memset(task_table, 0, sizeof(task_table)); // 清空任务表
}

// 调用该函数的地方不能有任何局部变量
// 调用前栈顶需要准备足够的空间
void task_to_user_mode(target_t target)
{
    task_t *task = running_task();
    
    task->vmap = kmalloc(sizeof(bitmap_t)); // 为任务分配虚拟内存位图结构体
    void *buf = (void *)alloc_kpage(1);     // 为位图缓冲区分配一页内存
    bitmap_init(task->vmap, buf, PAGE_SIZE, KERNEL_MEMORY_SIZE/PAGE_SIZE); // 初始化虚拟内存位图
    
    task->pde = (u32)copy_pde();            // 复制当前任务的页目录作为新任务的页目录
    set_cr3(task->pde);                     // 切换到新任务的页目录

    u32 addr = (u32)task + PAGE_SIZE; // 计算任务栈顶地址
    addr -= sizeof(intr_frame_t);     // 为中断帧留出空间
    intr_frame_t *frame = (intr_frame_t *)(addr); // 在栈顶创建中断帧

    frame->vector = 0x20;               // 中断向量号
    frame->edi = 1;                     // 初始化 edi 寄存器值
    frame->esi = 2;                     // 初始化 esi 寄存器值
    frame->ebp = 3;                     // 初始化 ebp 寄存器值
    frame->esp_dummy = 4;               // esp 占位
    frame->ebx = 5;                     // 初始化 ebx 寄存器值
    frame->edx = 6;                     // 初始化 edx 寄存器值
    frame->ecx = 7;                     // 初始化 ecx 寄存器值
    frame->eax = 8;                     // 初始化 eax 寄存器值

    frame->gs = 0;                     // 设置 GS 段选择子为空
    frame->ds = USER_DATA_SELECTOR;    // 设置数据段选择子
    frame->es = USER_DATA_SELECTOR;    // 设置附加段选择子
    frame->fs = USER_DATA_SELECTOR;    // 设置 FS 段选择子
    frame->ss = USER_DATA_SELECTOR;    // 设置栈段选择子
    frame->cs = USER_CODE_SELECTOR;    // 设置代码段选择子

    frame->error = ONIX_MAGIC;         // 错误码，设置魔数以便后续校验结构完整性

    u32 stack3 = alloc_kpage(1);                    // 为用户栈分配一页用户内存
    frame->eip = (u32)target;                       // 设置指令指针为目标函数地址
    frame->eflags = ((0 << 12) | 0b10 | 1 << 9);    // 设置标志寄存器，启用中断
    frame->esp = USER_STACK_TOP;                    // 设置用户栈指针为用户栈顶地址

    asm volatile(
        "movl %0, %%esp\n"                          // 设置栈指针 esp 指向中断帧
        "jmp interrupt_exit\n" :: "m"(frame)        // 跳转到中断退出处理程序，切换到用户模式
    );
}

extern void idle_thread();
extern void init_thread();
extern void test_thread();

void task_init(){
    list_init(&block_list); // 初始化任务阻塞链表
    list_init(&sleep_list); // 初始化任务睡眠链表
    task_setup();           // 初始化任务系统

    idle_task = task_create(idle_thread, "idle", 1, KERNEL_USER);   // 创建空闲任务
    task_create(init_thread, "init", 5, NORMAL_USER);               // 创建初始化任务
    task_create(test_thread, "test", 5, KERNEL_USER);               // 创建测试任务

    printk("Task init done!\n");
}
