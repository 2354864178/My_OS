#include <onix/fs.h>
#include <onix/stdlib.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/assert.h>
#include <onix/stat.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

#define INODE_NR 64    // i节点数量

static inode_t inodes[INODE_NR]; // i节点数组

// 获取一个空闲的i节点，返回i节点指针
static inode_t* get_free_inode(){   
    for(int i = 0; i < INODE_NR; i++){
        if(inodes[i].dev == EOF) return &inodes[i];
    }
    panic("No free inode");
}

// 释放i节点
static void put_free_inode(inode_t *inode){
    assert(inode != inodes);    // 不能释放根目录i节点
    assert(inode->count == 0);  // 只能释放引用计数为0的i节点
    inode->dev = EOF;           // 将设备号设为EOF，表示该i节点未被使用
}

// 获取根目录i节点
inode_t* get_root_inode(){
    return inodes;              // 根目录i节点在数组的第一个位置
}

// 计算i节点所在的块号
static inline idx_t inode_block(super_block_t *sb, idx_t nr){
    return 2 + sb->desc->s_imap_blocks + sb->desc->s_zmap_blocks + (nr-1)/BLOCK_INODES; // 计算i节点所在的块号
}

// 从已有的i节点中获取指定设备和i节点号的i节点，返回i节点指针
static inode_t *find_inode(dev_t dev, idx_t num){
    super_block_t *sb = get_super(dev); // 获取设备的超级块
    assert(sb); // 断言超级块存在
    list_t *list = &sb->inodes; // 获取超级块的i节点列表
    for(list_node_t *node = list->head.next; node != &list->tail; node = node->next){
        inode_t *inode = element_entry(inode_t, node, node); // 获取i节点指针
        if(inode->num == num) return inode; // 如果找到匹配的i节点，返回指针
    }
    return NULL;
}

// 从设备和i节点号获取i节点，返回i节点指针
inode_t *iget(dev_t dev, idx_t num){
    inode_t *inode = find_inode(dev, num); // 先尝试从已有的i节点中获取指定设备和i节点号的i节点
    if(inode){
        inode->count++;         // 如果找到，引用计数加1
        inode->atime = time();  // 更新最后访问时间
        return inode;           // 返回i节点指针
    }

    super_block_t *sb = get_super(dev); // 获取设备的超级块
    assert(sb); // 断言超级块存在
    assert(num > 0 && num <= sb->desc->s_ninodes); // 断言i节点号合法

    inode = get_free_inode();   // 获取一个空闲的i节点
    inode->dev = dev;           // 设置设备号
    inode->num = num;           // 设置i节点号
    inode->count = 1;           // 引用计数设为1，表示有一个使用者在使用这个i节点
    list_push(&sb->inodes, &inode->node);   // 将i节点加入超级块的i节点列表
    idx_t block = inode_block(sb, num);     // 计算i节点所在的块号
    buffer_t *buffer = bread(dev, block);   // 从设备读取i节点所在的块到缓冲区
    inode_desc_t *desc = (inode_desc_t *)buffer->data + (num-1)%BLOCK_INODES; // 计算i节点在块中的位置，获取i节点描述信息
    inode->desc = desc;         // 将i节点描述信息复制到i节点结构体中
    inode->buffer = buffer;     // 设置i节点所在的缓冲区

    inode->atime = time();     // 设置最后访问时间为当前时间
    inode->mtime = time();     // 设置最后修改时间为当前时间
    
    return inode;             // 返回i节点指针
}

void iput(inode_t *inode){
    if (!inode) return;

    if(inode -> buffer -> dirty){
        bwrite(inode->buffer);
    }

    inode->count--;

    if (inode->count) return; 
    
    brelse(inode->buffer);  // 释放i节点所在的缓冲区
    list_remove(&inode->node);  // 从超级块的i节点列表中移除该i节点
    put_free_inode(inode);  // 将i节点标记为未使用，释放内存

}

void inode_init(){
    for (size_t i = 0; i < INODE_NR; i++){
        inode_t *inode = &inodes[i];
        inode->dev = EOF;
    }
}

int inode_read(inode_t *inode, char *buf, size_t size, size_t offset){
    assert(inode); // 断言i节点存在
    assert(buf);   // 断言缓冲区存在
    assert(ISFILE(inode->desc->i_mode)); // 断言i节点是一个文件

    if(offset >= inode->desc->i_size) return EOF;

    u32 begin = offset;
    u32 left = MIN(size, inode->desc->i_size - offset);     // 计算实际需要读取的字节数，不能超过文件大小
    while(left){
        idx_t nr = bmap(inode, offset/BLOCK_SIZE, false);   // 计算当前读取位置所在的块索引
        assert(nr);    // 断言块索引有效
        buffer_t *buffer = bread(inode->dev, nr);       // 从设备读取块到缓冲区
        u32 start = offset % BLOCK_SIZE;                // 计算当前块内的起始位置
        u32 bytes = MIN(left, BLOCK_SIZE - start);      // 计算当前块内实际需要读取的字节数，不能超过块大小
        offset += bytes;    // 更新读取位置
        left -= bytes;      // 更新剩余字节数
        memcpy(buf, buffer->data + start, bytes);  // 将数据从缓冲区复制到用户缓冲区
        buf += bytes;       // 更新用户缓冲区指针
        brelse(buffer);     // 释放缓冲区        
    }
    inode->atime = time();  // 更新最后访问时间
    return offset-begin;    // 返回实际读取的字节数
}

int inode_write(inode_t *inode, const char *buf, size_t size, size_t offset){
    assert(inode); // 断言i节点存在
    assert(buf);   // 断言缓冲区存在
    assert(ISFILE(inode->desc->i_mode)); // 断言i节点是一个文件

    u32 begin = offset;
    u32 left = size;   // 计算实际需要写入的字节数
    while(left){
        idx_t nr = bmap(inode, offset/BLOCK_SIZE, true);    // 计算当前写入位置所在的块索引，如果块不存在则创建
        assert(nr);         // 断言块索引有效
        buffer_t *buffer = bread(inode->dev, nr);       // 从设备读取块到缓冲区
        buffer->dirty = true;                           // 标记缓冲区为脏，表示数据需要写回磁盘
        u32 start = offset % BLOCK_SIZE;                // 计算当前块内的起始位置
        u32 bytes = MIN(left, BLOCK_SIZE - start);      // 计算当前块内实际需要写入的字节数，不能超过块大小
        offset += bytes;    // 更新写入位置
        left -= bytes;      // 更新剩余字节数

        if(offset > inode->desc->i_size){
            inode->desc->i_size = offset;       // 如果写入位置超过文件大小，更新文件大小
            inode->buffer->dirty = true;        // 标记i节点所在的缓冲区为脏，表示需要写回磁盘
        }        

        memcpy(buffer->data + start, buf, bytes);  // 将数据从用户缓冲区复制到缓冲区
        buf += bytes;           // 更新用户缓冲区指针
        brelse(buffer);         // 释放缓冲区        
    }
    if(offset > inode->desc->i_size){   // 如果写入位置超过文件大小，更新文件大小
        inode->desc->i_size = offset;
    }
    inode->mtime = time();      // 更新最后修改时间
    return offset-begin;        // 返回实际写入的字节数
}


