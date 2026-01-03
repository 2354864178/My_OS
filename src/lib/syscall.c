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

// 内联汇编实现带一个参数的系统调用
static _inline u32 _syscall1(u32 nr, u32 arg1)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"   // 触发中断 0x80，进入内核态执行系统调用
        : "=a"(ret)     // 输出操作数，将 eax 寄存器的值存入 ret 变量
        : "a"(nr), "b"(arg1) ); // 输入操作数，将系统调用号放入 eax 寄存器，第一个参数放入 ebx 寄存器
    return ret;
}

static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2));
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3)
{
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

u32 test(){
    return _syscall0(SYS_NR_TEST);
}

void yield(){
    _syscall0(SYS_NR_YIELD);   
}

void sleep(u32 ms){
    _syscall1(SYS_NR_SLEEP, ms);
}

int32 write(fd_t fd, char *buf, u32 len){
    return _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

int32 brk(void *addr){
    return _syscall1(SYS_NR_BRK, (u32)addr);
}

pid_t getpid(){
    return _syscall0(SYS_NR_GETPID);
}

pid_t waitpid(pid_t pid, int *status){
    return _syscall2(SYS_NR_WAITPID, pid, (u32)status);
}

pid_t getppid(){
    return _syscall0(SYS_NR_GETPPID);
}

pid_t fork(){
    return _syscall0(SYS_NR_FORK);
}

void exit(int status){
    _syscall1(SYS_NR_EXIT, status);
}

time_t time(){
    return _syscall0(SYS_NR_TIME);
}
