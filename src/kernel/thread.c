#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/task.h>
#include <onix/arena.h>
#include <onix/stdio.h>

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

static void user_init_thread(){
    u32 counter = 0;
    BMB;
    char ch;
    while (true) {
        pid_t pid = fork();
        if (pid) {
            printf("Parent thread %d, %d, %d... \n", getpid(), getppid(), counter++);
        }
        else{
            printf("Child thread %d, %d, %d... \n", getpid(), getppid(), counter++);
            exit(0);
        }
        // hang();
        sleep(1000);
    }
}

// 初始化测试线程函数
void init_thread(){
    char temp[100];         // 临时缓冲区
    task_to_user_mode(user_init_thread);    // 切换到用户态运行
}

void test_thread(){
    set_interrupt_state(true);
    u32 counter = 0;
    while(true) {
        // void *ptr = kmalloc(1200);
        // LOGK("kmalloc 0x%p....\n", ptr);
        // kfree(ptr);

        // ptr = kmalloc(1024);
        // LOGK("kmalloc 0x%p....\n", ptr);
        // kfree(ptr);

        // ptr = kmalloc(54);
        // LOGK("kmalloc 0x%p....\n", ptr);
        // kfree(ptr);
        printf("Test thread %d, %d, %d... \n", getpid(), getppid(), counter++);
        sleep(2000);
    }
}
