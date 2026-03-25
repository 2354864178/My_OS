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

idx_t bmap(inode_t *inode, idx_t block, bool create){
    assert(block >= 0 && block < TOTAL_BLOCKS);

    buffer_t *buf = inode->buffer; // 获取i节点所在的缓冲区
    buf->count += 1; // 先给 inode buffer 加引用，防止被回收

    // 直接块 
    if (block < DIRECTORY_BLOCKS){
        u16 *array = inode->desc.i_zone; 
        
        if (!array[block] && create){
            array[block] = balloc(inode->dev);
            buf->dirty = true;
        }
        idx_t result = array[block]; // 先拿结果
        brelse(buf);                  // 再释放 buffer
        return result;
    }

    block -= DIRECTORY_BLOCKS;

    // 一阶间接块 (Single Indirect)
    if (block < INDIRECT1_BLOCKS){
        u16 *inode_array = inode->desc.i_zone;  // i节点描述信息中的块指针数组
        u16 indir_blkno = inode_array[DIRECTORY_BLOCKS];    // 一级间接块的块号

        if (!indir_blkno) {
            if (!create){
                brelse(buf);
                return 0;
            }
            indir_blkno = balloc(inode->dev);   // 分配新块
            inode_array[DIRECTORY_BLOCKS] = indir_blkno;    // 更新一级间接块的块号
        }
        buf->dirty = true;
        brelse(buf);
        buffer_t *indir_buf = bread(inode->dev, indir_blkno);
        u16 *indir_array = (u16 *)indir_buf->data;

        u16 data_blkno = indir_array[block];
        if (!data_blkno && create){
            data_blkno = balloc(inode->dev);
            indir_array[block] = data_blkno;
        }
        indir_buf->dirty = true;
        idx_t result = data_blkno;
        brelse(indir_buf);
        return result;  
    }

    block -= INDIRECT1_BLOCKS;
    assert(block < INDIRECT2_BLOCKS); // 断言块号合法

    // 二阶间接块 (Double Indirect)
    {
        u16 *inode_array = inode->desc.i_zone;
        u16 indir2_blkno = inode_array[DIRECTORY_BLOCKS + 1];
        int need_zero2 = 0;

        if (!indir2_blkno){
            if (!create){
                brelse(buf);
                return 0;
            }
            indir2_blkno = balloc(inode->dev);
            inode_array[DIRECTORY_BLOCKS + 1] = indir2_blkno;
        }
        buf->dirty = true;
        brelse(buf);
        buffer_t *indir2_buf = bread(inode->dev, indir2_blkno);
        u16 *indir2_array = (u16 *)indir2_buf->data;

        u16 idx1 = block / BLOCK_INDEXES;
        u16 idx0 = block % BLOCK_INDEXES;

        u16 indir1_blkno = indir2_array[idx1];
        int need_zero1 = 0;
        if (!indir1_blkno){
            if (!create){
                brelse(indir2_buf);
                return 0;
            }
            indir1_blkno = balloc(inode->dev);
            indir2_array[idx1] = indir1_blkno;
        }
        indir2_buf->dirty = true;
        brelse(indir2_buf);
        buffer_t *indir1_buf = bread(inode->dev, indir1_blkno);
        u16 *indir1_array = (u16 *)indir1_buf->data;
        
        u16 data_blkno = indir1_array[idx0];
        if (!data_blkno && create){
            data_blkno = balloc(inode->dev);
            indir1_array[idx0] = data_blkno;
        }
        indir1_buf->dirty = true;
        idx_t result = data_blkno;
        brelse(indir1_buf);
        return result;
    }
}
