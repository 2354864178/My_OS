#ifndef ONIX_NVME_H
#define ONIX_NVME_H

#include <onix/types.h>
#include <onix/mutex.h>

#define SECTOR_SIZE 512 // 扇区大小

#define NVME_CTRL_NR 2    // NVMe 控制器数量（当前按需）
#define NVME_DISK_NR 1    // 每个控制器的磁盘数量（先支持 nsid=1）
#define NVME_PART_NR 4    // 每个磁盘的主分区数量

#define NVME_ADMIN_Q_DEPTH 16   // 管理队列深度
#define NVME_IO_Q_DEPTH    16   // IO 队列深度

#pragma pack(1) 
typedef struct part_entry_t {   // 分区表项结构体
    u8 bootable;                // 是否可引导
    u8 start_head;              // 起始磁头
    u8 start_sector : 6;        // 起始扇区
    u16 start_cylinder : 10;    // 起始柱面
    u8 system;                  // 分区类型
    u8 end_head;                // 结束磁头
    u8 end_sector : 6;          // 结束扇区
    u16 end_cylinder : 10;      // 结束柱面
    u32 start;                  // 起始逻辑块地址
    u32 count;                  // 分区占用扇区数
} part_entry_t;
#pragma pack() 

#pragma pack(1) 
typedef struct boot_sector_t {              // 引导扇区结构体
    u8 bootstrap[446];                      // 引导代码
    part_entry_t entry[NVME_PART_NR];       // 分区表项
    u16 signature;                          // 结束标志 0x55AA
} boot_sector_t;
#pragma pack() 

typedef struct nvme_part_t {    // NVME 分区结构体
    char name[8];               // 分区名称
    struct nvme_disk_t *disk;    // 所属磁盘
    u32 system;                 // 分区类型
    u32 start;                  // 起始扇区
    u32 count;                  // 总扇区数
} nvme_part_t;

typedef struct nvme_disk_t {
    char name[8];               // 磁盘名称
    struct nvme_ctrl_t *ctrl;   // 所属控制器
    u32 nsid;                   // Namespace ID
    u32 total_sectors;          // 总 512B 扇区数（对齐 device 层）
    u32 lba_size;               // 当前 LBA 大小（字节），要求 512
    nvme_part_t disk[NVME_PART_NR]; // 主分区数组
} nvme_disk_t;

typedef struct nvme_ctrl_t { 
    char name[8];                       // 控制器名称
    u32 mmio_base;                      // NVMe BAR 映射后的 MMIO 基址（32 位内核要求 <4GiB）
    u32 db_stride;                      // doorbell stride（字节）
    raw_mutex_t lock;                   // 互斥锁
    nvme_disk_t disks[NVME_DISK_NR];    // 磁盘数组
    nvme_disk_t *selected_disk;         // 当前选择的磁盘

    // 提交/完成队列
    void *admin_sq;     // 提交队列
    void *admin_cq;     // 完成队列
    u16 admin_sq_tail;  // 提交队列尾指针
    u16 admin_cq_head;  // 完成队列头指针
    u8  admin_cq_phase; // 完成队列相位位

    // IO 队列
    void *io_sq;        // 提交队列
    void *io_cq;        // 完成队列
    u16 io_sq_tail;     // 提交队列尾指针
    u16 io_cq_head;     // 完成队列头指针
    u8  io_cq_phase;    // 完成队列相位位

    u16 next_cid;       // 下一个命令标识符
} nvme_ctrl_t;

// 磁盘操作
int nvme_pio_read(nvme_disk_t *disk, void *buffer, u8 count, idx_t lba);
int nvme_pio_write(nvme_disk_t *disk, void *buffer, u8 count, idx_t lba);
int nvme_pio_ioctl(nvme_disk_t *disk, int cmd, void *args, int flags);

// 分区操作
int nvme_pio_part_read(nvme_part_t *part, void *buffer, u8 count, idx_t lba);
int nvme_pio_part_write(nvme_part_t *part, void *buffer, u8 count, idx_t lba);
int nvme_pio_part_ioctl(nvme_part_t *part, int cmd, void *args, int flags);

void nvme_init(void);

#endif // ONIX_NVME_H