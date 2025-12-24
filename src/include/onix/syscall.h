#ifndef ONIX_SYSCALL_H
#define ONIX_SYSCALL_H

#include <onix/types.h>

typedef enum syscall_t{
    SYS_NR_TEST,
    SYS_NR_SLEEP,
    SYS_NR_YIELD,
    SYS_NR_WRITE,
    SYS_NR_BRK,
    SYS_NR_GETPID,
    SYS_NR_GETPPID,
} syscall_t;

u32 test();
void yield(); 
void sleep(u32 ms);  

int32 write(fd_t fd, char *buf, u32 len);
int32 brk(void *addr);

pid_t getpid();
pid_t getppid();

#endif