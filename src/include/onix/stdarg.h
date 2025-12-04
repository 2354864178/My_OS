#ifndef ONIX_STDARG_H
#define ONIX_STDARG_H

typedef char *va_list;

#define va_start(ap, v) (ap = (va_list)&v + sizeof(char *))     // 假设所有参数都按指针大小对齐
#define va_arg(ap, t) (*(t *) ((ap += sizeof(char *)) - sizeof(char *)))    // 获取下一个参数，并移动指针
#define va_end(ap) (ap = (va_list)0)    // 清理工作

#endif
