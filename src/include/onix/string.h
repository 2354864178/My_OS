#ifndef ONIX_STRING_H
#define ONIX_STRING_H

#include <onix/types.h>

char *strcpy(char *dest, const char *src);                  // 复制字符串
char *strncpy(char *dest, const char *src, size_t count);   // 复制指定长度字符串
char *strcat(char *dest, const char *src);      // 拼接字符串
size_t strlen(const char *str);                 // 计算字符串长度
size_t strnlen(const char *str, size_t maxlen);     // 计算指定长度字符串长度
int strcmp(const char *lhs, const char *rhs);       // 比较字符串
char *strchr(const char *str, int ch);      // 查找字符首次出现位置
char *strrchr(const char *str, int ch);     // 查找字符最后出现位置
char *strsep(const char *str);      // 查找分隔符首次出现位置
char *strrsep(const char *str);     // 查找分隔符最后出现位置

int memcmp(const void *lhs, const void *rhs, size_t count); // 比较内存区域
void *memset(void *dest, int ch, size_t count);             // 设置内存区域
void *memcpy(void *dest, const void *src, size_t count);    // 复制内存区域
void *memchr(const void *ptr, int ch, size_t count);        // 查找内存区域中字符首次出现位置

#endif