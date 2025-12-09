#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
raw_mutex_t mutex;   // 全局不可重入互斥锁
reentrant_mutex_t lock; // 全局可重入互斥锁

// 空闲线程函数
void idle_thread(){
    set_interrupt_state(true);  // 允许中断
    u32 count=0;
    while(true){
        // LOGK("Idle thread running... %d\n", count++);
        asm volatile(
            "std\n"     // 开中断
            "hlt\n"     // 进入休眠状态（关闭CPU），等待下一个中断
        );
        yield();    // 让出 CPU 控制权
    }
}

// 初始化测试线程函数
void init_thread(){
    // raw_mutex_init(&mutex);         // 初始化全局互斥锁
    reentrant_mutex_init(&lock);   // 初始化全局可重入互斥锁
    set_interrupt_state(true);  // 允许中断
    u32 count=0;
    while(true){
        reentrant_mutex_lock(&lock);
        // LOGK("Init thread running... %d\n", count++);
        sleep(500);
        reentrant_mutex_unlock(&lock);
    }
}

void test_thread(){
    set_interrupt_state(true);  // 允许中断
    u32 count=0;
    while(true){
        reentrant_mutex_lock(&lock);
        // LOGK("Test thread running... %d\n", count++);
        sleep(500);
        reentrant_mutex_unlock(&lock);
    }
}
