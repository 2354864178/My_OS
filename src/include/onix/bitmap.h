#ifndef ONIX_BITMAP_H
#define ONIX_BITMAP_H

#include <onix/types.h>

typedef struct bitmap_t
{
    u8 *bits;   // 位图缓冲区
    u32 length; // 位图缓冲区长度
    u32 offset; // 位图开始的偏移
} bitmap_t;


void bitmap_init(bitmap_t *map, char *bits, u32 length, u32 offset);    // 初始化位图

void bitmap_make(bitmap_t *map, char *bits, u32 length, u32 offset);    // 构造位图

bool bitmap_test(bitmap_t *map, u32 index);                             // 测试位图的某一位是否为 1

void bitmap_set(bitmap_t *map, u32 index, bool value);                  // 设置位图某位的值

int bitmap_scan(bitmap_t *map, u32 count);                              // 从位图中得到连续的 count 位为 0 的位置，返回起始位索引，失败返回 -1

#endif