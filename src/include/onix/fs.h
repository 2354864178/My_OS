#ifndef ONIX_FS_H
#define ONIX_FS_H

#include <onix/types.h>
#include <onix/list.h>
#include <onix/buffer.h>

#define BLOCK_SIZE 1024 // 块大小
#define SECTOR_SIZE 512 // 扇区大小

#define MINIX1_MAGIC 0x137F // Minix1 文件系统魔数
#define NAME_MAX 14         // 最长文件名长度  

#define IMAP_NR 8 // i节点位图占用的块数
#define ZMAP_NR 8 // 块位图占用的块数

#define BLOCK_BITS (BLOCK_SIZE * 8)                         // 每块包含的位数
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t))    // 每块包含的i节点数量
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t))      // 每块包含的目录项数量
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))            // 每块包含的索引数量

#define DIRECTORY_BLOCKS (7)    // 直接块数量
#define INDIRECT1_BLOCKS (BLOCK_INDEXES)    // 一级间接块数量
#define INDIRECT2_BLOCKS (INDIRECT1_BLOCKS * INDIRECT1_BLOCKS)  // 二级间接块数量
#define TOTAL_BLOCKS (DIRECTORY_BLOCKS + INDIRECT1_BLOCKS + INDIRECT2_BLOCKS) // 文件最大块数量

// i节点描述信息结构体 硬盘上的i节点表示  管理用途（存储文件的元数据，提供文件系统操作所需的信息）
typedef struct inode_desc_t{ 
    u16 i_mode; // 文件类型和权限
    u16 i_uid;  // 所有者用户ID
    u32 i_size; // 文件大小 
    u32 i_time; // 最后修改时间
    u8 i_gid;  // 所有者组ID
    u8 i_nlinks; // 链接数
    u16 i_zone[9]; // 数据块指针
} inode_desc_t;

// i节点结构体 内存中的i节点表示  管理用途（存储文件的元数据，提供文件系统操作所需的信息）
typedef struct inode_t{
    inode_desc_t desc;                  // i节点描述信息
    struct buffer_t* buffer;            // i节点所在的缓冲区
    dev_t dev;                          // 设备号
    idx_t num;                          // i节点号
    u32 count;                          // 引用计数  管理用途（维护i节点的使用情况，优化i节点分配和回收）
    time_t atime;                       // 最后访问时间  管理用途（提供文件系统操作所需的信息，允许访问文件系统中的其他文件和目录）
    time_t mtime;                       // 最后修改时间  管理用途（提供文件
    list_node_t node;                   // i节点列表节点  管理用途（维护i节点的使用情况，优化i节点分配和回收）
    dev_t mount_dev;                    // 挂载设备号  管理用途（表示文件系统中的挂载点，提供文件系统操作所需的信息）
} inode_t;

// 超级块描述信息结构体  硬盘上的超级块表示
typedef struct super_desc_t{
    u16 s_ninodes;          // i节点总数
    u16 s_nzones;           // 块总数
    u16 s_imap_blocks;      // i节点位图占用的块数
    u16 s_zmap_blocks;      // 块位图占用的块数
    u16 s_firstdatazone;    // 第一个数据块号
    u16 s_log_zone_size;    // 每个块包含的扇区数的对数值
    u32 s_max_size;         // 文件最大大小
    u16 s_magic;            // 文件系统魔数
    u16 s_state;            // 文件系统状态
} super_desc_t;

// 超级块结构体 内存中的超级块表示 
typedef struct super_block_t{
    super_desc_t *desc;                 // 超级块描述信息
    struct buffer_t* buffer;            // 超级块所在的缓冲区
    struct buffer_t* imaps[IMAP_NR];    // i节点位图缓冲区
    struct buffer_t* zmaps[ZMAP_NR];    // 块位图缓冲区
    dev_t dev;                          // 设备号
    list_t inodes;                      // i节点列表  管理用途（维护i节点的使用情况，优化i节点分配和回收）
    inode_t *iroot;                     // 根目录i节点  管理用途（提供文件系统的入口，允许访问文件系统中的其他文件和目录）
    inode_t *imount;                    // 挂载点i节点  管理用途（表示文件系统中的挂载点，提供文件系统操作所需的信息）
} super_block_t;

// 目录项结构体  管理用途（表示文件系统中的目录项，提供文件系统操作所需的信息）
typedef struct dentry_t{
    u32 inode;              // i节点号
    char name[NAME_MAX];    // 文件名
} dentry_t;

super_block_t* get_super(dev_t dev);    // 获取设备的超级块
super_block_t* read_super(dev_t dev);   // 从设备读取超级块

idx_t balloc(dev_t dev);            // 从设备上分配一个块，返回块号 
void bfree(dev_t dev, idx_t idx);   // 从设备上释放一个块，返回块号 
idx_t ialloc(dev_t dev);            // 从设备上分配一个i节点，返回i节点号 
void ifree(dev_t dev, idx_t idx);   // 从设备上释放一个i节点，返回i节点号 

inode_t *iget(dev_t dev, idx_t num);    // 从设备和i节点号获取i节点，返回i节点指针
void iput(inode_t *inode);              // 释放i节点 
inode_t* get_root_inode();              // 获取根目录i节点

#endif
