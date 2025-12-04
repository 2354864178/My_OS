#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 空闲线程函数
void idle_thread(){
    set_interrupt_state(true);  // 允许中断
    u32 count=0;
    while(true){
        LOGK("Idle thread running... %d\n", count++);
        asm volatile(
            "std\n"     // 开中断
            "hlt\n"     // 进入休眠状态（关闭CPU），等待下一个中断
        );
        yield();    // 让出 CPU 控制权
    }
}

// 初始化测试线程函数
void init_thread(){
    set_interrupt_state(true);  // 允许中断
    u32 count=0;
    while(true){
        LOGK("Init thread running... %d\n", count++);
        test();    // 调用测试系统调用
    }
}
