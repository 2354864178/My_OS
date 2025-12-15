#include <onix/arena.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/assert.h>

#define BUF_COUNT 4 // 堆内存缓存页数量

extern u32 free_pages;  // 空闲页数量
static arena_descriptor_t descriptors[DESC_COUNT];  // 内存块描述符数组

// 初始化内存块描述符
void arena_init(){
    u32 block_size = 16; // 最小块大小 16 字节
    for (size_t i = 0; i < DESC_COUNT; i++) {
        arena_descriptor_t *desc = &descriptors[i]; // 获取当前描述符指针
        desc->block_size = block_size;              // 设置块大小
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size; // 计算每页可分配块数量
        list_init(&desc->free_list);                // 初始化空闲块链表
        block_size *= 2;                            // 块大小翻倍
    }
}

// 获得 arena 第 idx 块内存指针
static void *get_arena_block(arena_t *arena, u32 idx){
    assert(arena->desc->total_block > idx);
    char *addr = (char *)arena + sizeof(arena_t); // 跳过 arena 头部
    u32 gap = idx * arena->desc->block_size;      // 计算块偏移
    return (void *)(addr + gap);                  // 返回块指针
}

// 根据内存块指针获得 arena 指针
static arena_t *get_block_arena(void *ptr){
    u32 addr = (u32)ptr;                        // 获取块地址
    addr &= 0xFFFFF000;                         // 地址向下取整到页起始处
    return (arena_t *)addr;
}

void *kmalloc(size_t size){
    arena_descriptor_t *desc = NULL;    // 描述符指针初始化为空
    arena_t *arena;                     // arena 指针初始化为空
    block_t *block;                     // 块指针初始化为空
    char *addr;                         // 返回地址指针初始化为空

    // 大于 1024 字节，按页分配
    if(size > 1024){
        u32 asize = size + sizeof(arena_t);                 // 计算实际需要的大小
        u32 page_count = div_round_up(asize, PAGE_SIZE);    // 计算需要的页数

        arena = (arena_t *)alloc_kpage(page_count);         // 分配对应页数的内存
        memset(arena, 0, page_count * PAGE_SIZE);           // 清零分配的内存
        arena->large = true;                                // 标记为大块
        arena->count = page_count;                          // 设置页数
        arena->desc = NULL;                                 // 大块没有描述符
        arena->magic = ONIX_MAGIC;                          // 设置魔数

        addr = (char *)((u32)arena + sizeof(arena_t));      // 计算返回地址
        return addr;
    }

    // 小于等于 1024 字节，按块分配
    // 找到合适的描述符
    for (size_t i = 0; i < DESC_COUNT; i++) {
        desc = &descriptors[i];                 // 获取当前描述符指针
        if(size <= desc->block_size) break;   // 找到合适的描述符就跳出循环

    }

    // assert(desc != NULL); // 必须找到合适的描述符

    // 描述符的空闲链表没有块，分配新的 arena
    if (list_empty(&desc->free_list)) {
        arena = (arena_t *)alloc_kpage(1);          // 分配一页内存
        memset(arena, 0, PAGE_SIZE);                // 清零分配
        arena->desc = desc;                         // 关联描述符
        arena->large = false;                       // 标记为小块
        arena->count = desc->total_block;           // 设置块数量
        arena->magic = ONIX_MAGIC;                  // 设置魔数

        // 将 arena 切分为块并加入描述符空闲链表
        for (size_t i = 0; i < desc->total_block; i++) {
            block = (block_t *)get_arena_block(arena, i);   // 获取块指针
            // assert(!list_search(&desc->free_list, block));  // 块不应已在链表中
            list_push(&arena->desc->free_list, block);      // 加
            // assert(list_search(&desc->free_list, block));   // 块应已在链表中
        }
    }
    
    block = (block_t *)list_pop(&desc->free_list);  // 从描述符空闲链表弹出一个块
    arena = get_block_arena(block);                 // 根据块指针获取 arena 指针

    assert(arena->magic == ONIX_MAGIC);             // 校验 arena 魔数
    arena->count--;                                 // 块数量减一

    return block;
}

void kfree(void *ptr){
    // assert(ptr);     // 指针不能为空
    arena_t *arena = get_block_arena(ptr);  // 根据块指针获取 arena 指针
    block_t *block = (block_t *)ptr;        // 转换块指针类型
    // assert(arena->magic == ONIX_MAGIC);     // 校验 arena 魔数
    assert(arena->large == 1 || arena->large == 0); // 校验 large 字段

    // 大块内存，按页释放
    if(arena->large){
        free_kpage((u32)arena, arena->count);     // 释放对应页数内存
        return;
    }

    // 小块内存，归还到描述符空闲链表
    list_push(&arena->desc->free_list, block);  // 将块加入描述符空闲链表
    arena->count++;                             // 块数量加一

    // 如果 arena 全部块都空闲且块数量大于 BUF_COUNT，释放该 arena
    if(arena->count == arena->desc->total_block){
        for(size_t i = 0; i < arena->desc->total_block; i++){
            block = get_arena_block(arena, i);                     // 获取块指针
            // assert(list_search(&arena->desc->free_list, block));   // 块应在链表中
            list_remove(block);                                    // 从链表中移除块
            // assert(!list_search(&arena->desc->free_list, block));  // 块不应在链表中
        }
        free_kpage((u32)arena, 1); // 释放该 arena 所在页
    }
}
