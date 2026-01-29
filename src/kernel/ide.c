#include <onix/ide.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/string.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/devicetree.h>
#include <onix/printk.h>
#include <onix/device.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

// IDE 寄存器基址
#define IDE_REG_PRIMARY 0x1F0   // 主基址
#define IDE_REG_SECONDARY 0x170 // 副基址

// IDE 寄存器偏移
#define IDE_REG_DATA 0x0000           // 数据寄存器
#define IDE_REG_ERROR 0x0001          // 错误寄存器（只读）
#define IDE_REG_FEATURES 0x0001       // 特性寄存器（只写）
#define IDE_REG_SECTOR_COUNT 0x0002   // 扇区数寄存器
#define IDE_REG_LBA_LOW 0x0003        // LBA 低 8 位
#define IDE_REG_LBA_MID 0x0004        // LBA 中 8 位
#define IDE_REG_LBA_HIGH 0x0005       // LBA 高 8 位
#define IDE_REG_HDDEVSEL 0x0006       // 选择硬盘设备寄存器
#define IDE_REG_STATUS 0x0007         // 状态寄存器（只读）
#define IDE_REG_COMMAND 0x0007        // 命令寄存器（只写）
#define IDE_REG_ALTSTATUS 0x0206     // 备用状态寄存器（只读）
#define IDE_REG_CONTROL 0x0206       // 控制寄存器（只写）
#define IDE_REG_DEVCONTROL 0x0206    // 设备控制寄存器（只写）

// IDE 命令
#define IDE_CMD_READ  0x20          // 读命令
#define IDE_CMD_WRITE 0x30          // 写命令
#define IDE_CMD_IDENTIFY 0xEC       // 识别命令

// IDE 状态码
#define IDE_SR_NULL 0x00    // 空状态
#define IDE_SR_BSY  0x80    // 忙碌状态
#define IDE_SR_DRDY 0x40    // 驱动器就绪
#define IDE_SR_DF   0x20    // 驱动器故障
#define IDE_SR_DRQ  0x08    // 数据请求
#define IDE_SR_ERR  0x01    // 错误状态

// IDE 错误码
#define IDE_ER_AMNF 0x01    // 地址标记未找到
#define IDE_ER_TK0NF 0x02   // 磁道 0 未找到
#define IDE_ER_ABRT 0x04    // 命令中止
#define IDE_ER_MCR 0x08     // 媒体更改请求
#define IDE_ER_IDNF 0x10    // ID 未找到
#define IDE_ER_MC 0x20      // 媒体错误
#define IDE_ER_UNC 0x40     // 未纠正错误
#define IDE_ER_BBK 0x80     // 坏块

#define IDE_LBA_MASTER 0xE0  // LBA 主设备选择器基址
#define IDE_LBA_SLAVE  0xF0  // LBA 从设备选择

typedef struct ide_dt_info
{
    bool present;
    u32 cmd_base;
    u32 cmd_size;
    u32 ctrl_base;
    u32 ctrl_size;
    u32 irq;
} ide_dt_info_t;

static ide_dt_info_t ide_dt[IDE_CTRL_NR];

static void ide_dt_probe_one(int idx, const char *paths[], size_t pathnr, u32 def_cmd, u32 def_ctrl, u32 def_irq)
{
    void *val; u32 len; ide_dt_info_t *info = &ide_dt[idx];

    if (dtb_get_prop_any(paths, pathnr, "reg", &val, &len) == 0 && len >= 8)
    {
        u32 *cells = (u32 *)val;
        info->cmd_base = dt_be32_read(&cells[0]);
        info->cmd_size = dt_be32_read(&cells[1]);
        if (len >= 16)
        {
            info->ctrl_base = dt_be32_read(&cells[2]);
            info->ctrl_size = dt_be32_read(&cells[3]);
        }
        info->present = true;
        LOGK("DT ide%d: cmd 0x%x size 0x%x (code 0x%x size 0x%x) \n",
             idx, info->cmd_base, info->cmd_size, def_cmd, 8U);
        LOGK("DT ide%d: ctrl 0x%x size 0x%x (code 0x%x size 0x%x) \n",
             idx, info->ctrl_base, info->ctrl_size, def_ctrl, 1U);
    }

    if (dtb_get_prop_any(paths, pathnr, "interrupts", &val, &len) == 0 && len >= 4)
    {
        info->irq = dt_be32_read(val);
        info->present = true;
        LOGK("DT ide%d: irq %u (code %u) \n",
             idx, info->irq, def_irq);
    }
}

static void ide_dt_probe(void)
{
    const char *p0[] = {"/ide@1f0"};
    const char *p1[] = {"/ide@170"};
    ide_dt_probe_one(0, p0, 1, IDE_REG_PRIMARY, IDE_REG_PRIMARY + IDE_REG_CONTROL, IRQ_HARDDISK);
    ide_dt_probe_one(1, p1, 1, IDE_REG_SECONDARY, IDE_REG_SECONDARY + IDE_REG_CONTROL, IRQ_HARDDISK2);
    printk("\n");
}

typedef enum PART_FS{      // 分区类型
    PART_FS_FAT12 = 1,      // FAT12 
    PART_FS_EXTENDED = 5,   // 扩展分区
    PART_FS_MINIX = 0x80,   // Minix 
    PART_FS_LINUX = 0x83,   // Linux 
} PART_FS;

// IDE 参数结构体
typedef struct ide_params_t
{
    u16 config;                 // 0 通用配置位
    u16 cylinders;              // 01 柱面数
    u16 RESERVED;               // 02 保留
    u16 heads;                  // 03 磁头数
    u16 RESERVED[5 - 3];        // 05 保留
    u16 sectors;                // 06 每磁道扇区数
    u16 RESERVED[9 - 6];        // 09 保留
    u8 serial[20];              // 10 ~ 19 序列号
    u16 RESERVED[22 - 19];      // 20 ~ 22 保留
    u8 firmware[8];             // 23 ~ 26 固件版本
    u8 model[40];               // 27 ~ 46 型号
    u8 drq_sectors;             // 47 传输扇区数
    u8 RESERVED[3];             // 48 保留
    u16 capabilities;           // 49 能力标志
    u16 RESERVED[59 - 49];      // 50 ~ 59 保留
    u32 total_lba;              // 60 ~ 61 总逻辑扇区数（LBA）
    u16 RESERVED;               // 62 保留
    u16 mdma_mode;              // 63 MDMA 模式
    u8 RESERVED;                // 64 保留
    u8 pio_mode;                // 64 PIO 模式
    u16 RESERVED[79 - 64];      // 65 ~ 79 保留（参见 ATA 规范）
    u16 major_version;          // 80 主版本
    u16 minor_version;          // 81 次版本
    u16 commmand_sets[87 - 81]; // 82 ~ 87 支持的命令集
    u16 RESERVED[118 - 87];     // 88 ~ 118 保留
    u16 support_settings;       // 119 支持设置
    u16 enable_settings;        // 120 启用设置
    u16 RESERVED[221 - 120];    // 121 ~ 221 保留
    u16 transport_major;        // 222 传输主版本
    u16 transport_minor;        // 223 传输次版本
    u16 RESERVED[254 - 223];    // 224 ~ 254 保留
    u16 integrity;              // 校验和
} _packed ide_params_t;

ide_ctrl_t ide_ctrls[IDE_CTRL_NR];

// IDE 中断处理程序
void ide_handler(int vector) {
    send_eoi(vector); // 发送中断结束信号
    ide_ctrl_t *ctrl = &ide_ctrls[vector - IRQ_HARDDISK - 0x20];    // 获取对应的 IDE 控制器
    u8 state = inb(ctrl->io_base + IDE_REG_STATUS);                 // 读取状态寄存器

    LOGK("%s: IDE Interrupt, Status: 0x%02X\n", ctrl->name, state);
    if(ctrl->wait_task) {
        task_unlock(ctrl->wait_task);   // 唤醒等待任务
        ctrl->wait_task = NULL;         // 清除等待任务
    }
}

static u32 ide_error(ide_ctrl_t *ctrl) {
    // 读取错误状态
    u8 error = inb(ctrl->io_base + IDE_REG_ERROR); // 读取错误寄存器
    if(error & IDE_ER_AMNF) LOGK("Address Mark Not Found\n");   // 地址标记未找到
    if(error & IDE_ER_TK0NF) LOGK("Track 0 Not Found\n", ctrl->name);       // 磁道 0 未找到
    if(error & IDE_ER_ABRT) LOGK("Command Aborted\n", ctrl->name);          // 命令中止
    if(error & IDE_ER_MCR) LOGK("Media Change Request\n");      // 媒体更改请求
    if(error & IDE_ER_IDNF) LOGK("ID Not Found\n");             // ID 未找到
    if(error & IDE_ER_MC) LOGK("Media Error\n");                // 媒体错误
    if(error & IDE_ER_UNC) LOGK("Uncorrectable Error\n");       // 未纠正错误
    if(error & IDE_ER_BBK) LOGK("Bad Block\n");                 // 坏块
    // return error;
}

static u32 ide_wait_busy(ide_ctrl_t *ctrl, u8 mask) {
    while(true) {
        u8 status = inb(ctrl->io_base + IDE_REG_ALTSTATUS); // 读取状态寄存器
        LOGK("%s: IDE Status: 0x%02X\n", ctrl->name, status);
        if(status & IDE_SR_ERR) ide_error(ctrl);    // 检查并报告错误
        if(status & IDE_SR_BSY)  continue;          // 仍然忙碌，继续等待   
        if((status & mask) == mask) return 0;       // 所需状态达成
    }
}

// 选择磁盘
static void ide_select_drive(ide_disk_t *disk) {
    outb(disk->ctrl->io_base + IDE_REG_HDDEVSEL, disk->selecter);
    disk->ctrl->selected_disk = disk;   // 更新当前选择的磁盘
}

// 选择扇区
static void ide_select_sector(ide_disk_t *disk, idx_t lba, u8 count) {
    outb(disk->ctrl->io_base + IDE_REG_FEATURES, 0);                            // 清除特性寄存器
    outb(disk->ctrl->io_base + IDE_REG_SECTOR_COUNT, count);                    // 设置扇区数
    outb(disk->ctrl->io_base + IDE_REG_LBA_LOW, (u8)(lba & 0xFF));              // 设置 LBA 低 8 位
    outb(disk->ctrl->io_base + IDE_REG_LBA_MID, (u8)((lba >> 8) & 0xFF));       // 设置 LBA 中 8 位
    outb(disk->ctrl->io_base + IDE_REG_LBA_HIGH, (u8)((lba >> 16) & 0xFF));     // 设置 LBA 高 8 位

    outb(disk->ctrl->io_base + IDE_REG_HDDEVSEL, disk->selecter | ((lba >> 24) & 0x0F)); // 设置 LBA 最高 4 位
}

static void ide_pio_read_sector(ide_disk_t *disk, u16 *buffer) {
    // 从数据寄存器读取一个扇区的数据
    for(size_t i = 0; i < SECTOR_SIZE/2; i++){
        buffer[i] = inw(disk->ctrl->io_base + IDE_REG_DATA); // 读取 16 位数据
    }
}

static void ide_pio_write_sector(ide_disk_t *disk, u16 *buffer) {
    // 向数据寄存器写入一个扇区的数据
    for(size_t i = 0; i < SECTOR_SIZE/2; i++){
        outw(disk->ctrl->io_base + IDE_REG_DATA, buffer[i]); // 写入 16 位数据
    }
}

int ide_pio_read(ide_disk_t *disk, void *buffer, u8 count, idx_t lba) {
    // disk: 目标磁盘
    // buffer: 数据缓冲区
    // count: 要读取的扇区数
    // lba: 起始逻辑块地址
    assert(count > 0);
    assert(!get_interrupt_state()); // 异步，确保在关中断状态下运行
    ide_ctrl_t *ctrl = disk->ctrl;

    raw_mutex_lock(&ctrl->lock); // 获取互斥锁

    ide_select_drive(disk);                     // 选择磁盘
    ide_wait_busy(ctrl, IDE_SR_DRDY);           // 等待驱动器就绪
    ide_select_sector(disk, lba, count);        // 选择扇区

    outb(ctrl->io_base + IDE_REG_COMMAND, IDE_CMD_READ); // 发送读命令

    for(size_t i = 0; i < count; i++) {
        task_t *current = running_task(); // 获取当前任务
        // 阻塞当前任务，等待数据准备好
        if(current->state == TASK_RUNNING){
            ctrl->wait_task = current; // 设置等待任务
            task_block(current, NULL, TASK_BLOCKED);       // 阻塞当前任务
        }
        ide_wait_busy(ctrl, IDE_SR_DRQ);            // 等待数据请求
        u32 offset = (u32)buffer + i * SECTOR_SIZE; // 计算缓冲区偏移
        ide_pio_read_sector(disk, (u16 *)offset);   // 读取一个扇区数据
    }

    raw_mutex_unlock(&ctrl->lock); // 释放互斥锁
    return 0; // 读取成功
}

int ide_pio_write(ide_disk_t *disk, void *buffer, u8 count, idx_t lba) {
    // disk: 目标磁盘
    // buffer: 数据缓冲区
    // count: 要写入的扇区数
    // lba: 起始逻辑块地址
    assert(count > 0);
    assert(!get_interrupt_state()); // 异步，确保在关中断状态下运行
    ide_ctrl_t *ctrl = disk->ctrl;

    raw_mutex_lock(&ctrl->lock); // 获取互斥锁

    ide_select_drive(disk);                     // 选择磁盘
    ide_wait_busy(ctrl, IDE_SR_DRDY);           // 等待驱动器就绪
    ide_select_sector(disk, lba, count);        // 选择扇区

    outb(ctrl->io_base + IDE_REG_COMMAND, IDE_CMD_WRITE); // 发送写命令

    for(size_t i = 0; i < count; i++) {
        u32 offset = (u32)buffer + i * SECTOR_SIZE; // 计算缓冲区偏移
        ide_pio_write_sector(disk, (u16 *)offset);  // 写入一个扇区数据

        task_t *current = running_task(); // 获取当前任务
        // 阻塞当前任务，等待数据准备好
        if(current->state == TASK_RUNNING){
            ctrl->wait_task = current; // 设置等待任务
            task_block(current, NULL, TASK_BLOCKED);       // 阻塞当前任务
        }
        LOGK("%s: Write sector %u done, waiting for completion...\n", ctrl->name, lba + i);
        task_sleep(100);                            // 等待 100 毫秒，确保数据写入完成
        ide_wait_busy(ctrl, IDE_SR_NULL);           // 等待写入完成
    }

    raw_mutex_unlock(&ctrl->lock); // 释放互斥锁
    return 0; // 写入成功
}

// IDE 磁盘的 ioctl 操作
int ide_pio_ioctl(ide_disk_t *disk, int cmd, void *args, int flags) {
    // disk: 目标磁盘
    // cmd: 控制命令
    // args: 命令参数
    // flags: 控制标志
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return 0;
    case DEV_CMD_SECTOR_COUNT:
        return disk->total_sectors;
    default:
        panic("ide_pio_ioctl: unsupported cmd %d\n", cmd);
        break;
    }
}

// 基于分区的读操作
int ide_pio_part_read(ide_part_t *part, void *buffer, u8 count, idx_t lba) {
    return ide_pio_read(part->disk, buffer, count, part->start + lba);
}

// 基于分区的写操作
int ide_pio_part_write(ide_part_t *part, void *buffer, u8 count, idx_t lba) {
    return ide_pio_write(part->disk, buffer, count, part->start + lba);
}

int ide_pio_part_ioctl(ide_part_t *part, int cmd, void *args, int flags) {
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return part->start;
    case DEV_CMD_SECTOR_COUNT:
        return part->count;
    default:
        panic("ide_pio_part_ioctl: unsupported cmd %d\n", cmd);
        break;
    }
}

// 交换字节序对
void ide_swap_pair(char *buf, u32 len){
    for(u32 i = 0; i < len; i += 2){
        char temp = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = temp;
    }
}

// 识别磁盘
static u32 ide_identify(ide_disk_t *disk, u16 *buf) {
    // disk: 目标磁盘
    // buf: 数据缓冲区
    LOGK("%s: IDENTIFY Disk\n", disk->name);
    raw_mutex_lock(&disk->ctrl->lock);          // 获取互斥锁
    ide_select_drive(disk);                     // 选择磁盘
    outb(disk->ctrl->io_base + IDE_REG_COMMAND, IDE_CMD_IDENTIFY); // 发送识别命令
    ide_wait_busy(disk->ctrl, IDE_SR_NULL);     // 等待 BSY 清除
    ide_params_t *params = (ide_params_t *)buf; // 参数结构体指针
    ide_pio_read_sector(disk, buf);       // 读取识别数据
    LOGK("disk %s total lba %d\n", disk->name, params->total_lba);

    u32 ret = EOF;
    if(params->total_lba == 0) goto rollback;   // 无效磁盘

    // 交换并打印识别信息
    ide_swap_pair(params->serial, sizeof(params->serial));          // 交换序列号字节序对
    LOGK("%s: Serial Number: %.20s\n", disk->name, params->serial); 
    ide_swap_pair(params->firmware, sizeof(params->firmware));      // 交换固件版本字节序对
    LOGK("%s: Firmware Version: %.8s\n", disk->name, params->firmware);
    ide_swap_pair(params->model, sizeof(params->model));            // 交换型号字节序对
    LOGK("%s: Model Number: %.40s\n\n", disk->name, params->model);

    // 设置磁盘参数
    disk->total_sectors = params->total_lba;            // 设置总扇区数
    disk->cylinders = params->cylinders;                // 设置柱面数
    disk->heads = params->heads;                        // 设置磁头数
    disk->sectors_per_track = params->sectors;          // 设置每磁道扇区数
    ret = 0;

rollback:
    raw_mutex_unlock(&disk->ctrl->lock);    // 释放互斥锁
    return ret;
}

// 初始化分区
static void ide_part_init(ide_disk_t *disk, u16 *buf){
    // disk: 目标磁盘
    // buf: 数据缓冲区

    if(!disk->total_sectors) return;            // 无效磁盘，直接返回
    ide_pio_read(disk, buf, 1, 0);              // 读取主引导扇区
    boot_sector_t *bs = (boot_sector_t *)buf;   // 引导扇区结构体指针

    for(size_t i=0; i<IDE_PART_NR; i++){
        part_entry_t *entry = &bs->entry[i];    // 分区表项指针
        ide_part_t *part = &disk->disk[i];      // 分区结构体指针
        if(!entry->system) continue;            // 分区类型为 0，跳过

        sprintf(part->name, "%s%d", disk->name, i + 1); // 设置分区名称
        
        LOGK("part %s \n", part->name);             // 打印分区信息
        LOGK("bootable: %d\n", entry->bootable);    // 是否可引导
        LOGK("  system: %x\n", entry->system);      // 分区类型
        LOGK("  start: %d\n", entry->start);        // 起始扇区
        LOGK("  count: %d\n\n", entry->count);      // 总扇区数

        part->disk = disk;              // 设置所属磁盘
        part->system = entry->system;   // 设置分区类型
        part->start = entry->start;     // 设置起始扇区
        part->count = entry->count;     // 设置总扇区数
        
        // 处理扩展分区
        if(part->system == PART_FS_EXTENDED){
            LOGK("unsupported Extended Partition");
            boot_sector_t *eboot = (boot_sector_t *)(buf + SECTOR_SIZE);    // 扩展引导扇区结构体指针
            ide_pio_read(disk, eboot, 1, part->start);                      // 读取扩展引导扇区
            for(size_t j=0; j<IDE_PART_NR; j++){
                part_entry_t *eentry = &eboot->entry[j];                    // 扩展分区表项指针
                if (!eentry->count) continue;                               // 分区类型为 0，跳过

                LOGK("part %d extend %d\n", i, j);              // 打印扩展分区信息
                LOGK("    bootable %d\n", eentry->bootable);    // 是否可引导
                LOGK("    start %d\n", eentry->start);          // 起始扇区
                LOGK("    count %d\n", eentry->count);          // 总扇区数
                LOGK("    system 0x%x\n\n", eentry->system);      // 分区类型
            }
        }
    }
}

static void ide_ctrl_init(void) {
    u16 *buf = (u16*)alloc_kpage(1);                // 分配一页内核页面作为缓冲区
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t *ctrl = &ide_ctrls[cidx];        // 获取控制器结构体
        sprintf(ctrl->name, "ide%d", cidx);         // 设置控制器名称
        raw_mutex_init(&ctrl->lock);                // 初始化互斥锁
        ctrl->selected_disk = NULL;                 // 初始化当前选择的磁盘为空
        ctrl->wait_task = NULL;                     // 初始化等待任务为空
            
        if(cidx == 0)  ctrl->io_base = ide_dt[0].cmd_base;  // 主控制器基址
        else ctrl->io_base = ide_dt[1].cmd_base;            // 副控制器基址

        ctrl->control = inb(ctrl->io_base + IDE_REG_CONTROL); // 读取控制寄存器初始值

        for(size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            ide_disk_t *disk = &ctrl->disks[didx];      // 获取磁盘结构体
            sprintf(disk->name, "hd%c", 'a' + cidx * IDE_DISK_NR + didx);   // 设置磁盘名称
            disk->ctrl = ctrl;                          // 设置所属控制器
            if(didx){
                disk->selecter = IDE_LBA_SLAVE;        
                disk->master = false;
            }
            else{
                disk->selecter = IDE_LBA_MASTER;
                disk->master = true;
            }
            ide_identify(disk, buf);
            ide_part_init(disk, buf);
        }
    }
    free_kpage((u32)buf, 1); // 释放内核页面
}

static void ide_install(){
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t *ctrl = &ide_ctrls[cidx];        // 获取控制器结构体
        for(size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            ide_disk_t *disk = &ctrl->disks[didx];      // 获取磁盘结构体
            if(!disk->total_sectors) continue;          // 无效磁盘，跳过
            dev_t dev = device_install(DEV_BLOCK, DEV_IDE_DISK, disk, disk->name, 0,
                ide_pio_ioctl, ide_pio_read, ide_pio_write );   // 安装磁盘设备
            for(size_t pidx = 0; pidx < IDE_PART_NR; pidx++) {
                ide_part_t *part = &disk->disk[pidx];   // 获取分区结构体
                if(!part->count) continue;              // 无效分区，跳过
                device_install(DEV_BLOCK, DEV_IDE_PART, part, part->name, dev,
                    ide_pio_part_ioctl, ide_pio_part_read, ide_pio_part_write ); // 安装分区设备
            }
        }
    }
}

void ide_init(void) {
    // LOGK("IDE Init Start...\n");
    // assert(dtb_node_enabled("/ide@1f0") && dtb_node_enabled("/ide@170"));
    ide_dt_probe();
    ide_ctrl_init();  // 初始化 IDE 控制器
    ide_install();    // 安装 IDE 设备

    set_interrupt_handler(IRQ_HARDDISK, ide_handler);   // 设置 IDE 中断处理程序
    set_interrupt_handler(IRQ_HARDDISK2, ide_handler);  // 设置第二个 IDE 中断处理程序
    set_interrupt_mask(IRQ_HARDDISK, true);             // 允许 IDE 中断
    set_interrupt_mask(IRQ_HARDDISK2, true);            // 允许第二个 IDE 中断
    // set_interrupt_mask(IRQ_CASCADE, true);              // 允许级联中断
}
