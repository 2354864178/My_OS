#ifndef ONIX_BUFFER_H
#define ONIX_BUFFER_H

#include <onix/types.h>
#include <onix/list.h>
#include <onix/mutex.h>

#define BLOCK_SIZE 1024                       // 块大小
#define SECTOR_SIZE 512                       // 扇区大小
#define BLOCK_SECS (BLOCK_SIZE / SECTOR_SIZE) // 一块占 2 个扇区

#define HASH_COUNT 31      // 需要是个素数
#define MAX_BUF_COUNT 4096 // 最大缓冲数量

typedef struct buffer_t
{
    char *data;         // 数据区
    dev_t dev;          // 设备号
    idx_t block;        // 块号
    int count;          // 引用计数， 表示有多少使用者在使用这个buffer
    list_node_t hnode;  // 哈希节点  管理用途（快速定位buffer，减少磁盘IO）
    list_node_t rnode;  // 缓冲列表节点  管理用途（维护LRU列表，优化缓存替换策略）
    raw_mutex_t lock;   // 锁
    bool dirty;         // 是否与磁盘不一致
    bool valid;         // 是否有效
} buffer_t;

buffer_t *bread(dev_t dev, idx_t block); // 从设备dev的块block读取数据到缓冲区，返回缓冲区指针
void bwrite(buffer_t *buffer);             // 将缓冲区数据写回设备
void brelse(buffer_t *buffer);             // 释放缓冲区，减少引用计数，可能会被回收

#endif