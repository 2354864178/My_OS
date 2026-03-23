#include <onix/string.h>
#include <onix/buffer.h>
#include <onix/fs.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/bitmap.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

// 从设备上分配一个块，返回块号
idx_t balloc(dev_t dev){
    super_block_t *sb = get_super(dev);
    
    buffer_t *buffer = NULL;
    bitmap_t map;
    idx_t bit=EOF;
    for(int i = 0; i < ZMAP_NR; i++){
        buffer = sb->zmaps[i];
        assert(buffer);

        bitmap_make(&map, buffer->data, BLOCK_SIZE, i*BLOCK_BITS+sb->desc->s_firstdatazone-1);

        bit = bitmap_scan(&map, 1);
        if(bit != EOF){             // 找到一个空闲块
            assert(bit >= 0 && bit < sb->desc->s_nzones);
            buffer->dirty = true;   // 标记为脏块，表示需要写回设备
            break;
        }
    }
    bwrite(buffer); // 将修改后的位图写回设备(用于调试)
    LOGK("balloc: allocated block %d\n", bit); // 输出分配的块号
    return bit;
}

// 从设备上释放一个块，返回块号
void bfree(dev_t dev, idx_t idx){
    super_block_t *sb = get_super(dev);
    
    buffer_t *buffer = NULL;
    bitmap_t map;
    for(int i = 0; i < ZMAP_NR; i++){
        
        if(idx > BLOCK_BITS * (i + 1)) continue; // 继续寻找对应的位图块
        buffer = sb->zmaps[i];
        assert(buffer);
        bitmap_make(&map, buffer->data, BLOCK_SIZE, i*BLOCK_BITS+sb->desc->s_firstdatazone-1);

        assert(bitmap_test(&map, idx)); // 该块必须已经被占用
        bitmap_set(&map, idx, 0);       // 置为 0,表示释放该块
        buffer->dirty = true;           // 标记为脏块，表示需要写回设备
        break;
    }
    bwrite(buffer); // 将修改后的位图写回设备(用于调试)
    LOGK("bfree: freed block %d\n", idx); // 输出释放的块号
}

// 从设备上分配一个i节点，返回i节点号
idx_t ialloc(dev_t dev){
    super_block_t *sb = get_super(dev);
    
    buffer_t *buffer = NULL;
    bitmap_t map;
    idx_t bit=EOF;
    for(int i = 0; i < IMAP_NR; i++){
        buffer = sb->imaps[i];
        assert(buffer);

        bitmap_make(&map, buffer->data, BLOCK_SIZE, i*BLOCK_BITS);

        bit = bitmap_scan(&map, 1);
        if(bit != EOF){             // 找到一个空闲i节点
            assert(bit >= 0 && bit < sb->desc->s_ninodes);
            buffer->dirty = true;   // 标记为脏块，表示需要写回设备
            break;
        }
    }
    bwrite(buffer); // 将修改后的位图写回设备(用于调试)
    LOGK("ialloc: allocated inode %d\n", bit); // 输出分配的i节点号
    return bit;
}

// 从设备上释放一个i节点，返回i节点号
void ifree(dev_t dev, idx_t idx){
    super_block_t *sb = get_super(dev);
    
    buffer_t *buffer = NULL;
    bitmap_t map;
    for(int i = 0; i < IMAP_NR; i++){
        
        if(idx > BLOCK_BITS * (i + 1)) continue; // 继续寻找对应的位图块
        buffer = sb->imaps[i];
        assert(buffer);
        bitmap_make(&map, buffer->data, BLOCK_SIZE, i*BLOCK_BITS);

        assert(bitmap_test(&map, idx)); // 该i节点必须已经被占用
        bitmap_set(&map, idx, 0);       // 置为 0,表示释放该i节点
        buffer->dirty = true;           // 标记为脏块，表示需要写回设备
        break;
    }
    bwrite(buffer); // 将修改后的位图写回设备(用于调试)
    LOGK("ifree: freed inode %d\n", idx); // 输出释放的i节点号
}
