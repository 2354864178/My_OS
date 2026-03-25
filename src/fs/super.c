#include <onix/fs.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/device.h>
#include <onix/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

#define SUPER_NR 16     // 超级块数量

static super_block_t super_blocks[SUPER_NR];    // 超级块数组
static super_block_t *root_super;               // 根文件系统的超级块指针

static super_block_t* get_free_super(){
    for(int i = 0; i < SUPER_NR; i++){
        if(super_blocks[i].dev == EOF) return &super_blocks[i];
    }
    panic("No free super block");
}

super_block_t* get_super(dev_t dev){
    for(int i = 0; i < SUPER_NR; i++){
        if(super_blocks[i].dev == dev) return &super_blocks[i];
    }
    return NULL;
}

super_block_t* read_super(dev_t dev){
    super_block_t* super = get_super(dev);  // 先检查是否已经存在该设备的超级块
    if(super) return super;                 // 已经存在，直接返回
    LOGK("Reading super block from device %d\n", dev);

    super = get_free_super();
    buffer_t* buffer = bread(dev, 1);       
    super->buffer = buffer;
    super->desc = (super_desc_t *)buffer->data; // 从缓冲区数据读取超级块描述信息
    super->dev = dev;
    
    assert(super->desc->s_magic == MINIX1_MAGIC); // 验证超级块魔数，确保文件系统类型正确

    memset(super->imaps, 0, sizeof(super->imaps)); // 初始化i节点位图缓冲区指针数组
    memset(super->zmaps, 0, sizeof(super->zmaps)); // 初始化块位图缓冲区指针数组
    
    int idx = 2; // i节点位图从第2块开始
    for(int i = 0; i < super->desc->s_imap_blocks; i++){
        assert(i<IMAP_NR); // 节点位图块数不超过预定义的最大值
        if(super->imaps[i] = bread(dev, idx)) idx++;    // 读取i节点位图块到缓冲区，并更新索引
        else break;
        
    }
    
    for(int i = 0; i < super->desc->s_zmap_blocks; i++){
        assert(i<ZMAP_NR); // 块位图块数不超过预定义的最大值
        if(super->zmaps[i] = bread(dev, idx)) idx++;    // 读取块位图块到缓冲区，并更新索引
        else break;
    }

    return super;
}

static void mount_root(){
    LOGK("Mounting root file system\n");
    device_t *device = device_find(DEV_NVME_PART, 0); // 查找第一个NVMe分区设备
    assert(device); // 确保设备存在

    root_super = read_super(device->dev); // 读取根文件系统的超级块

    root_super->iroot = iget(root_super->dev, 1);   // 获取根目录i节点，i节点号为1
    root_super->imount = root_super->iroot;         // 设置挂载点i节点为根目录i节点

    idx_t idx = 0;
    inode_t *inode = iget(root_super->dev, 1);      // 从设备和i节点号获取根目录i节点，返回i节点指针

    // 直接块
    idx = bmap(inode, 3, true);

    // 一级间接块
    idx = bmap(inode, 7 + 7, true);

    // 二级间接块
    idx = bmap(inode, 7 + 512 * 3 + 510, true);

    iput(inode);
}

void super_init(){
    for(int i = 0; i < SUPER_NR; i++){
        super_block_t *super = &super_blocks[i];
        super->dev = EOF; // 初始化超级块数组，标记所有超级块为未使用状态
        super->buffer = NULL;
        super->desc = NULL;
        super->iroot = NULL;
        super->imount = NULL;
        list_init(&super->inodes); // 初始化超级块的i节点列表
    }
    mount_root(); // 挂载根文件系统
}
