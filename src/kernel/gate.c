#include <onix/interrupt.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/syscall.h>
#include <onix/task.h>
#include <onix/memory.h>
#include <onix/ide.h>
#include <onix/string.h>


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

extern ide_ctrl_t ide_ctrls[IDE_CTRL_NR];
static u32 sys_test(){          // 测试系统调用函数
    // BMB;
    // u16 *buf = (u16 *)alloc_kpage(1); // 分配一页内核页面
    // LOGK("pio read buf addr: 0x%p\n", buf);
    // ide_disk_t *disk = &ide_ctrls[0].disks[0]; // 获取第一个 IDE 磁盘
    // ide_pio_read(disk, buf, 4, 0); // 读取前 4 个扇区到缓冲区
    // BMB;

    // memset(buf, 0x5a, 512); // 将缓冲区清零
    // ide_pio_write(disk, buf, 1, 1); // 写入第 1 个扇区
    // free_kpage((u32)buf, 1); // 释放内核页面

    return 255;
}

// 系统调用处理函数：写数据
int32 sys_write(fd_t fd, char *buf, u32 len){
    // fd: 文件描述符
    // buf: 数据缓冲区指针
    // len: 要写入的数据长度
    
    if(fd == stdout || fd == stderr) return console_write(buf, len);    // 写入控制台
    panic("Unsupported fd %d in sys_write", fd);    // 不支持的文件描述符，触发 panic
    return 0;
}

extern time_t sys_time(); // 声明 sys_time 函数

void syscall_init(){            // 初始化系统调用处理函数表
    for (int i = 0; i < SYSCALL_SIZE; i++) {
        syscall_table[i] = (handler_t)syscall_default;  // 默认指向默认处理函数
    }

    syscall_table[SYS_NR_TEST] = (handler_t)sys_test;       // 注册测试系统调用处理函数
    syscall_table[SYS_NR_SLEEP] = (handler_t)task_sleep;    // 注册任务睡眠的系统调用处理函数
    syscall_table[SYS_NR_YIELD] = (handler_t)task_yield;    // 注册任务让出 CPU 的系统调用处理函数
    syscall_table[SYS_NR_WRITE] = (handler_t)sys_write;     // 注册写数据的系统调用处理函数
    syscall_table[SYS_NR_BRK] = (handler_t)sys_brk;         // 注册 brk 系统调用处理函数
    syscall_table[SYS_NR_GETPID] = (handler_t)sys_getpid;   // 注册获取进程ID的系统调用处理函数
    syscall_table[SYS_NR_GETPPID] = (handler_t)sys_getppid; // 注册获取父进程ID的系统调用处理函数
    syscall_table[SYS_NR_FORK] = (handler_t)task_fork;      // 注册 fork 系统调用处理函数
    syscall_table[SYS_NR_EXIT] = (handler_t)task_exit;      // 注册 exit 系统调用处理函数
    syscall_table[SYS_NR_WAITPID] = (handler_t)task_waitpid; // 注册 waitpid 系统调用处理函数
    syscall_table[SYS_NR_TIME] = (handler_t)sys_time;       // 注册 time 系统调用处理函数
    LOGK("Syscall init done!\n");
}

