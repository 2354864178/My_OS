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

static void real_init_thread(){
    u32 counter = 0;

    char ch;
    while (true) {
        BMB;
        printf("Init thread running... %d\n", counter++);
        // LOGK("Init thread running... %d\n", counter++);
        sleep(1000);
    }
}

// 初始化测试线程函数
void init_thread(){
    char temp[100];         // 临时缓冲区
    task_to_user_mode(real_init_thread);    // 切换到用户态运行
}

void test_thread(){
    set_interrupt_state(true);  // 允许中断
    reentrant_mutex_init(&lock);
    u32 count=0;
    while(true){
        reentrant_mutex_lock(&lock);
        LOGK("Test thread running... %d\n", count++);
        sleep(1000);
        reentrant_mutex_unlock(&lock);
    }
}
