#ifndef ONIX_MUTEX_H
#define ONIX_MUTEX_H

#include <onix/types.h>
#include <onix/list.h>

// 基础不可重入互斥锁
typedef struct raw_mutex_t {
    bool lock_state;      // 锁状态：false=空闲（未锁定），true=占用（已锁定）
    list_t wait_queue;    // 等待队列：存放加锁失败阻塞的任务
} raw_mutex_t;

// 可重入互斥锁（基于raw_mutex_t封装）
typedef struct reentrant_mutex_t {
    struct task_t *owner;         // 锁持有者：当前持有锁的任务ID
    raw_mutex_t base_mutex;       // 底层基础互斥锁（实现核心互斥逻辑）
    u32 reentrant_count;          // 重入次数：同一任务重复加锁的次数
} reentrant_mutex_t;

// 函数声明（语义对齐）
void raw_mutex_init(raw_mutex_t *raw_mutex);
void raw_mutex_lock(raw_mutex_t *raw_mutex);
void raw_mutex_unlock(raw_mutex_t *raw_mutex);

void reentrant_mutex_init(reentrant_mutex_t *rt_mutex);
void reentrant_mutex_lock(reentrant_mutex_t *rt_mutex);
void reentrant_mutex_unlock(reentrant_mutex_t *rt_mutex);


#endif
