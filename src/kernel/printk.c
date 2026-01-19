#include <onix/stdarg.h>
#include <onix/console.h>
#include <onix/stdio.h>

static char buf[1024];
extern int32 console_write();

int printk(const char *fmt, ...)    // 内核打印函数
{
    va_list args;   // 可变参数列表
    int i;  

    va_start(args, fmt);            // 初始化可变参数列表
    i = vsprintf(buf, fmt, args);   // 格式化字符串
    va_end(args);                   // 清理可变参数列表
    console_write(NULL, buf, i);    // 输出到控制台
    return i;
}