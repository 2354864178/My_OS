#ifndef ONIX_IDE_H
#define ONIX_IDE_H

#include <onix/types.h>
#include <onix/mutex.h>

#define SECTOR_SIZE 512 // 扇区大小

#define IDE_CTRL_NR 2    // IDE 控制器数量
#define IDE_DISK_NR 2    // 每个控制器的磁盘数量
#define IDE_PART_NR 4    // 每个磁盘的主分区数量

#pragma pack(1) 
typedef struct part_entry_t {  // 分区表项结构体
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
typedef struct boot_sector_t { // 引导扇区结构体
    u8 bootstrap[446];          // 引导代码
    part_entry_t entry[IDE_PART_NR];      // 分区表项
    u16 signature;              // 结束标志 0x55AA
} boot_sector_t;
#pragma pack() 

typedef struct ide_part_t {    // IDE 分区结构体
    char name[8];               // 分区名称
    struct ide_disk_t *disk;    // 所属磁盘
    u32 system;                 // 分区类型
    u32 start;                  // 起始扇区
    u32 count;                  // 总扇区数
} ide_part_t;

typedef struct ide_disk_t {
    char name[8];               // 磁盘名称
    struct ide_ctrl_t *ctrl;    // 所属控制器
    u8 selecter;                // 选择器
    bool master;                // 是否为主盘
    u32 total_sectors;          // 总扇区数
    u32 cylinders;              // 柱面数
    u32 heads;                  // 磁头数
    u32 sectors_per_track;      // 每磁道扇区数
    ide_part_t disk[IDE_PART_NR]; // 主分区数组
} ide_disk_t;

typedef struct ide_ctrl_t {
    char name[8];                   // 控制器名称
    u16 io_base;                    // I/O 端口基址
    raw_mutex_t lock;               // 互斥锁
    ide_disk_t disks[IDE_DISK_NR];  // 磁盘数组
    ide_disk_t *selected_disk;      // 当前选择的磁盘
    u8 control;                     // 控制寄存器值
    struct task_t *wait_task;       // 等待任务
} ide_ctrl_t;

int ide_pio_read(ide_disk_t *disk, void *buffer, u8 count, idx_t lba);
int ide_pio_write(ide_disk_t *disk, void *buffer, u8 count, idx_t lba);

#endif // ONIX_IDE_H