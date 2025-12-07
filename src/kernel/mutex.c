#include <onix/mutex.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/assert.h>

void mutex_init(mutex_t *mutex){
    mutex->value = false;       // 初始状态为未锁定
    list_init(&mutex->waiters); // 初始化等待队列
}

void mutex_lock(mutex_t *mutex){
    bool intr = interrupt_disable();    // 关闭中断
    task_t *current = running_task();   // 获取当前任务

    while(mutex->value){
        // 锁已被占用，将当前任务加入等待队列并阻塞
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }

    assert(!mutex->value);  // 确保锁未被占用
    mutex->value++;         // 占用锁
    assert(mutex->value);   // 确保锁已被占用

    set_interrupt_state(intr);  // 恢复中断状态
}

void mutex_unlock(mutex_t *mutex){
    bool intr = interrupt_disable();    // 关闭中断

    assert(mutex->value); // 确保锁已被占用
    mutex->value--;       // 释放锁

    // 唤醒等待队列中的第一个任务
    if(!list_empty(&mutex->waiters)){
        task_t *task = element_entry(task_t, node, mutex->waiters.tail.prev);   // 获取等待队列中的第一个任务
        assert(task->magic == ONIX_MAGIC);  // 确保任务结构体未损坏
        task_unlock(task);     // 唤醒任务
        task_yield();           // 释放锁后立即让出CPU，避免锁竞争
    }

    set_interrupt_state(intr);  // 恢复中断状态
}   
