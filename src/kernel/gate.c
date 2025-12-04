#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SYSCALL_SIZE 64                 // 系统调用数量
handler_t syscall_table[SYSCALL_SIZE];  // 系统调用处理函数表

void syscall_check(u32 nr){     // 检查系统调用号是否合法
    if(nr >= SYSCALL_SIZE){
        panic("Syscall number %d exceed max %d", nr, SYSCALL_SIZE);
    }
}

static void syscall_default(){      // 默认系统调用处理函数
    panic("Default syscall handler called!");
}

task_t *task = NULL;    // 测试系统调用使用的任务指针
static u32 syscall_test(){          // 测试系统调用函数
    if(!task){
        task = running_task();      // 获取当前运行的任务指针
        task_block(task, NULL, TASK_BLOCKED);   // 强制阻塞当前任务
    }
    else{
        task_unlock(task);          // 解锁任务
        task = NULL;                // 重置任务指针
    }
    return 255;
}

extern void task_yield();        // 任务让出 CPU 的系统调用接口

void syscall_init(){        // 初始化系统调用处理函数表
    for (int i = 0; i < SYSCALL_SIZE; i++) {
        syscall_table[i] = (handler_t)syscall_default;  // 默认指向默认处理函数
    }

    syscall_table[SYS_NR_TEST] = syscall_test;  // 注册测试系统调用处理函数
    syscall_table[SYS_NR_YIELD] = task_yield;   // 注册任务让出 CPU 的系统调用处理函数
    LOGK("Syscall init done!\n");
}

