#include <onix/syscall.h>

// 内联汇编实现无参数系统调用
static _inline u32 _syscall0(u32 nr)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"   // 触发中断 0x80，进入内核态执行系统调用
        : "=a"(ret)     // 输出操作数，将 eax 寄存器的值存入 ret 变量
        : "a"(nr));     // 输入操作数，将系统调用号放入 eax 寄存器
    return ret;
}

u32 test(){
    return _syscall0(SYS_NR_TEST);
}

void yield(){
    _syscall0(SYS_NR_YIELD);   
}
