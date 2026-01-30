#include <onix/mutex.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/assert.h>
#include <onix/list.h>
#include <onix/onix.h>
#include <onix/types.h>

// 不可重入互斥锁
void raw_mutex_init(raw_mutex_t *raw_mutex) {
    raw_mutex->lock_state = false;      // 初始状态：未锁定
    list_init(&raw_mutex->wait_queue);  // 初始化等待队列
}

void raw_mutex_lock(raw_mutex_t *raw_mutex) {
    bool intr = interrupt_disable();    // 关闭中断（保证临界区原子性）
    task_t *current = running_task();   // 获取当前运行任务

    // 锁被占用时，阻塞当前任务并加入等待队列
    while (raw_mutex->lock_state) {
        task_block(current, &raw_mutex->wait_queue, TASK_BLOCKED);
    }

    assert(!raw_mutex->lock_state);  // 确保锁未被占用
    raw_mutex->lock_state = true;    // 占用锁
    assert(raw_mutex->lock_state);   // 确保锁已成功占用

    set_interrupt_state(intr);  // 恢复原中断状态
}

void raw_mutex_unlock(raw_mutex_t *raw_mutex) {
    bool intr = interrupt_disable();    // 关闭中断（保证临界区原子性）

    assert(raw_mutex->lock_state); // 确保锁处于占用状态
    raw_mutex->lock_state = false; // 释放锁

    // 唤醒等待队列中的第一个任务
    if (!list_empty(&raw_mutex->wait_queue)) {
        task_t *task = element_entry(task_t, node, raw_mutex->wait_queue.tail.prev);    // 获取等待队列中的第一个任务
        assert(task->magic == ONIX_MAGIC);  // 任务结构体未损坏
        task_unlock(task);      // 唤醒阻塞的任务
        task_yield();           // 让出CPU，避免锁竞争
    }

    set_interrupt_state(intr);  // 恢复原中断状态
}

// 可重入互斥锁
void reentrant_mutex_init(reentrant_mutex_t *rt_mutex) {
    rt_mutex->owner = NULL;             // 初始无持有者
    rt_mutex->reentrant_count = 0;      // 初始重入次数为0
    raw_mutex_init(&rt_mutex->base_mutex); // 初始化底层基础锁
}


void reentrant_mutex_lock(reentrant_mutex_t *rt_mutex) {
    task_t *current = running_task();   // 获取当前运行任务

    // 非重入场景：首次加锁
    if (rt_mutex->owner != current) {
        raw_mutex_lock(&rt_mutex->base_mutex); // 获取底层基础锁
        rt_mutex->owner = current;             // 标记当前任务为持有者
        assert(rt_mutex->reentrant_count == 0); // 断言：重入次数初始为0
        rt_mutex->reentrant_count = 1;         // 重入次数设为1
        return;
    }

    // 重入场景：仅增加重入次数
    rt_mutex->reentrant_count++;
}

void reentrant_mutex_unlock(reentrant_mutex_t *rt_mutex) {
    task_t *current = running_task();
    assert(rt_mutex->owner == current);  // 当前任务是锁持有者

    // 重入场景：仅减少重入次数，不释放底层锁
    if (rt_mutex->reentrant_count > 1) {
        rt_mutex->reentrant_count--;
        return;
    }

    // 非重入场景：释放底层锁并清空持有者
    assert(rt_mutex->reentrant_count == 1);     // 最后一次重入
    rt_mutex->owner = NULL;                     // 清空持有者
    rt_mutex->reentrant_count = 0;              // 重入次数清零
    raw_mutex_unlock(&rt_mutex->base_mutex);    // 释放底层基础锁
}
