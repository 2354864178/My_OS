#include <onix/buffer.h>
#include <onix/memory.h>
#include <onix/arena.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static u32 buffer_count = 0; // 当前缓冲数量
static buffer_t *buffer_start = (buffer_t *)KERNEL_BUFFER_MEM; // 内核缓冲区起始地址
static buffer_t *buffer_ptr = (buffer_t *)KERNEL_BUFFER_MEM;   // 当前可用的缓冲区指针
static void *buffer_data = (void *)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE); // 当前可用的缓冲区数据区指针

static list_t free_list;                 // 空闲缓冲链表，管理空闲的buffer
static list_t wait_list;                 // 等待链表，管理等待buffer的任务
static list_t hash_table[HASH_COUNT];    // 哈希表，管理所有buffer

// 哈希函数，根据设备号和块号计算哈希值
u32 hash(dev_t dev, idx_t block){
    return (dev ^ block) % HASH_COUNT;
}

// 从哈希表中找指定设备的块儿
static buffer_t *get_from_hash_table(dev_t dev, idx_t block){
    u32 idx = hash(dev, block);         // 计算哈希值
    list_t *list = &hash_table[idx];    // 获取哈希表对应链表
    buffer_t *bf = NULL;                // 定义buffer指针

    // 遍历链表，寻找匹配的buffer
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next){
        buffer_t *ptr = element_entry(buffer_t, hnode, node); 
        if (ptr->dev == dev && ptr->block == block){
            bf = ptr;       // 如果找到，就返回buffer的指针
            break;
        }
    }

    if (!bf) return NULL;   // 如果没有找到，返回NULL

    // 如果找到的buffer在空闲链表中，说明它正在被使用，需要从空闲链表中移除
    if (list_search(&free_list, &bf->rnode)) list_remove(&bf->rnode); 

    return bf;
}

// 将buffer插入哈希表
static void hash_locate(buffer_t *bf){
    u32 idx = hash(bf->dev, bf->block);     // 计算哈希值
    list_t *list = &hash_table[idx];        // 获取哈希表对应链表
    assert(!list_search(list, &bf->hnode)); // 断言buffer不在哈希表中，避免重复插入
    list_push(list, &bf->hnode);
}

// 将buffer从哈希表中移除
static void hash_remove(buffer_t *bf){
    u32 idx = hash(bf->dev, bf->block);
    list_t *list = &hash_table[idx];
    assert(list_search(list, &bf->hnode));
    list_remove(&bf->hnode);
}

// 获取一个新的buffer
static buffer_t *get_new_buffer(){
    buffer_t *bf = NULL;    // 定义buffer指针

    // 如果指针的位置小于data的位置，说明还有空间可以分配新的buffer
    if ((u32)buffer_ptr + sizeof(buffer_t) < (u32)buffer_data){
        bf = buffer_ptr;        // 将当前指针位置的buffer分配给bf
        bf->data = buffer_data; // 将当前数据指针赋值给buffer的data字段
        bf->dev = EOF;          // 初始化设备号为EOF，表示未分配
        bf->block = 0;          // 初始化块号为0
        bf->count = 0;          // 初始化引用计数为0
        bf->dirty = false;      // 初始化dirty标志为false，表示数据与磁盘一致
        bf->valid = false;      // 初始化valid标志为false，表示buffer无效
        raw_mutex_init(&bf->lock);   // 初始化buffer的锁

        buffer_count++; // 增加缓冲数量 
        buffer_ptr++;   // 将指针移动到下一个buffer位置
        buffer_data -= BLOCK_SIZE;  // data向后移动

        LOGK("buffer count %d\n", buffer_count);
    }
    return bf;
}

// 获取一个空闲的buffer
static buffer_t *get_free_buffer(){
    buffer_t *bf = NULL;
    while (true){
        bf = get_new_buffer();  // 首先尝试分配一个新的buffer
        if (bf) return bf;      // 如果成功分配到新的buffer，直接返回

        // 否则，如果空闲链表不为空，说明有可用的buffer，可以从空闲链表中获取
        if (!list_empty(&free_list)) {
            bf = element_entry(buffer_t, rnode, list_popback(&free_list));  // 取最远未访问过的块
            hash_remove(bf);    // 从哈希表中移除该buffer，准备重新分配
            bf->valid = false;  // 标记该buffer无效，表示它的数据不再与磁盘一致
            return bf;
        }
        task_block(running_task(), &wait_list, TASK_BLOCKED);   // 等待某个缓冲释放
    }
}

// 获取指定设备和块号的buffer
buffer_t *getblk(dev_t dev, idx_t block){
    buffer_t *bf = get_from_hash_table(dev, block);  // 先尝试从hash表获取，如果已有缓冲直接返回
    if (bf) return bf;

    bf = get_free_buffer(); // 如果没有缓冲，则获取一个新的buffer
    assert(bf->count == 0);
    assert(bf->dirty == 0);

    bf->count = 1;      // 引用计数设为1，表示有一个使用者在使用这个buffer
    bf->dev = dev;      // 设置设备号
    bf->block = block;  // 设置块号
    hash_locate(bf);    // 放到hash表
    return bf;
}

// 从设备读取指定块到buffer中
buffer_t *bread(dev_t dev, idx_t block){
    buffer_t *bf = getblk(dev, block); // 获取缓冲
    assert(bf != NULL);

    // 如果缓冲有效，说明数据已经在缓冲中，不需要从设备读取，直接返回
    if (bf->valid){
        bf->count++;
        return bf;
    }

    // 如果缓冲无效做块设备请求，从硬盘读取到buffer中
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);

    bf->dirty = false;
    bf->valid = true;
    return bf;
}

// 将buffer中的数据写回设备
void bwrite(buffer_t *bf){
    assert(bf);
    if (!bf->dirty) return; // 如果buffer没有被修改过，不需要写回设备，直接返回
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);
    bf->dirty = false;
    bf->valid = true;
}

// 释放buffer
void brelse(buffer_t *bf){
    if (!bf) return;
    if(bf->dirty) bwrite(bf); // 如果buffer被修改过，先写回设备
    bf->count--;                // 引用计数减去1
    assert(bf->count >= 0);

    if(bf->count > 0) return; // 如果还有使用者在使用这个buffer，直接返回
    assert(!bf->rnode.next && !bf->rnode.prev); // 断言buffer不在空闲链表中
    list_push(&free_list, &bf->rnode); // 否则将buffer放回空闲链表
    // 唤醒等待链表中的任务，通知有buffer可用
    if(!list_empty(&wait_list)) task_unlock(element_entry(task_t, node, wait_list.head.next));
}

void buffer_init(){
    LOGK("buffer_t size is %d\n", sizeof(buffer_t));
    list_init(&free_list);  // 初始化空闲链表
    list_init(&wait_list);  // 初始化等待进程链表

    // 初始化哈希表
    for(size_t i = 0; i < HASH_COUNT; i++) list_init(&hash_table[i]);
}
