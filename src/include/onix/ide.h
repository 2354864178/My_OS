#ifndef ONIX_IDE_H
#define ONIX_IDE_H

#include <onix/types.h>
#include <onix/mutex.h>

#define SECTOR_SIZE 512 // 扇区大小

#define IDE_CTRL_NR 2    // IDE 控制器数量
#define IDE_DISK_NR 2    // 每个控制器的磁盘数量

typedef struct ide_disk_t {
    char name[8];               // 磁盘名称
    struct ide_ctrl_t *ctrl;    // 所属控制器
    u8 selecter;                // 选择器
    bool master;                // 是否为主盘
    u32 total_sectors;          // 总扇区数
    u32 cylinders;              // 柱面数
    u32 heads;                  // 磁头数
    u32 sectors_per_track;      // 每磁道扇区数
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