#ifndef ONIX_MUTEX_H
#define ONIX_MUTEX_H

#include <onix/types.h>
#include <onix/list.h>

typedef struct mutex_t{
    bool value;      // 锁的状态，true表示已锁定，false表示未锁定
    list_t waiters;  // 等待队列
} mutex_t;

void mutex_init(mutex_t *mutex);     // 初始化互斥锁
void mutex_lock(mutex_t *mutex);     // 加锁
void mutex_unlock(mutex_t *mutex);   // 解锁

#endif
