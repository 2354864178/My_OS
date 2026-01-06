#include <onix/ide.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/string.h>
#include <onix/stdio.h>
#include <onix/assert.h>
#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)  // 内核日志宏

// IDE 寄存器基址
#define IDE_REG_PRIMARY 0x1F0   // 主基址
#define IDE_REG_SECONDARY 0x170 // 副基址

// IDE 寄存器偏移
#define IDE_REG_DATA 0x00           // 数据寄存器
#define IDE_REG_ERROR 0x01          // 错误寄存器（只读）
#define IDE_REG_FEATURES 0x01       // 特性寄存器（只写）
#define IDE_REG_SECTOR_COUNT 0x02   // 扇区数寄存器
#define IDE_REG_LBA_LOW 0x03        // LBA 低 8 位
#define IDE_REG_LBA_MID 0x04        // LBA 中 8 位
#define IDE_REG_LBA_HIGH 0x05       // LBA 高 8 位
#define IDE_REG_HDDEVSEL 0x06       // 选择硬盘设备寄存器
#define IDE_REG_STATUS 0x07         // 状态寄存器（只读）
#define IDE_REG_COMMAND 0x07        // 命令寄存器（只写）
#define IDE_REG_ALTSTATUS 0x206     // 备用状态寄存器（只读）
#define IDE_REG_CONTROL 0x206       // 控制寄存器（只写）
#define IDE_REG_DEVCONTROL 0x206    // 设备控制寄存器（只写）

// IDE 命令
#define IDE_CMD_READ  0x20          // 读命令
#define IDE_CMD_WRITE 0x30          // 写命令
#define IDE_CMD_IDENTIFY 0xEC       // 识别命令

// IDE 状态码
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

ide_ctrl_t ide_ctrls[IDE_CTRL_NR];

// IDE 中断处理程序
void ide_handler(int vector) {
    send_eoi(vector); // 发送中断结束信号
    ide_ctrl_t *ctrl = &ide_ctrls[vector - IRQ_HARDDISK - 0x20];    // 获取对应的 IDE 控制器
    u8 state = inb(ctrl->io_base + IDE_REG_STATUS);                 // 读取状态寄存器

    LOGK("%s: IDE Interrupt, Status: 0x%02X\n", ctrl->name, state);
    if(ctrl->wait_task) {
        task_unlock(ctrl->wait_task); // 唤醒等待任务
        ctrl->wait_task = NULL;        // 清除等待任务
    }
}

static u32 ide_error(ide_ctrl_t *ctrl) {
    // 读取错误状态
    u8 error = inb(ctrl->io_base + IDE_REG_ERROR); // 读取错误寄存器
    if(error & IDE_ER_AMNF) LOGK("%s: Address Mark Not Found\n", ctrl->name);   // 地址标记未找到
    if(error & IDE_ER_TK0NF) LOGK("%s: Track 0 Not Found\n", ctrl->name);       // 磁道 0 未找到
    if(error & IDE_ER_ABRT) LOGK("%s: Command Aborted\n", ctrl->name);          // 命令中止
    if(error & IDE_ER_MCR) LOGK("%s: Media Change Request\n", ctrl->name);      // 媒体更改请求
    if(error & IDE_ER_IDNF) LOGK("%s: ID Not Found\n", ctrl->name);             // ID 未找到
    if(error & IDE_ER_MC) LOGK("%s: Media Error\n", ctrl->name);                // 媒体错误
    if(error & IDE_ER_UNC) LOGK("%s: Uncorrectable Error\n", ctrl->name);       // 未纠正错误
    if(error & IDE_ER_BBK) LOGK("%s: Bad Block\n", ctrl->name);                 // 坏块
    return error;
}

static u32 ide_wait_busy(ide_ctrl_t *ctrl, u8 mask) {
    while(true) {
        u8 status = inb(ctrl->io_base + IDE_REG_ALTSTATUS); // 读取状态寄存器
        if(status & IDE_SR_ERR) ide_error(ctrl);    // 检查并报告错误
        if(status & IDE_SR_BSY) continue;           // 等待 BSY 清除
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

static void ide_pio_read_sector(ide_ctrl_t *ctrl, u16 *buffer) {
    // 从数据寄存器读取一个扇区的数据
    for(size_t i = 0; i < SECTOR_SIZE/2; i++){
        buffer[i] = inw(ctrl->io_base + IDE_REG_DATA); // 读取 16 位数据
    }
}

static void ide_pio_write_sector(ide_ctrl_t *ctrl, u16 *buffer) {
    // 向数据寄存器写入一个扇区的数据
    for(size_t i = 0; i < SECTOR_SIZE/2; i++){
        outw(ctrl->io_base + IDE_REG_DATA, buffer[i]); // 写入 16 位数据
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
        ide_pio_read_sector(ctrl, (u16 *)offset);   // 读取一个扇区数据
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
        ide_pio_write_sector(ctrl, (u16 *)offset);  // 写入一个扇区数据

        task_t *current = running_task(); // 获取当前任务
        // 阻塞当前任务，等待数据准备好
        if(current->state == TASK_RUNNING){
            ctrl->wait_task = current; // 设置等待任务
            task_block(current, NULL, TASK_BLOCKED);       // 阻塞当前任务
        }
        ide_wait_busy(ctrl, IDE_SR_DRQ);            // 等待数据请求
    }

    raw_mutex_unlock(&ctrl->lock); // 释放互斥锁
    return 0; // 写入成功
}

static void ide_ctrl_init(void) {
    for(size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t *ctrl = &ide_ctrls[cidx];        // 获取控制器结构体
        sprintf(ctrl->name, "ide%d", cidx);         // 设置控制器名称
        raw_mutex_init(&ctrl->lock);                // 初始化互斥锁
        ctrl->selected_disk = NULL;                 // 初始化当前选择的磁盘为空
        
        if(cidx == 0)  ctrl->io_base = IDE_REG_PRIMARY; // 主控制器基址
        else ctrl->io_base = IDE_REG_SECONDARY;         // 副控制器基址
        
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
        }
    }
}

void ide_init(void) {
    LOGK("IDE Init Start...\n");
    ide_ctrl_init();  // 初始化 IDE 控制器

    set_interrupt_handler(IRQ_HARDDISK, ide_handler);   // 设置 IDE 中断处理程序
    set_interrupt_handler(IRQ_HARDDISK2, ide_handler);  // 设置第二个 IDE 中断处理程序
    set_interrupt_mask(IRQ_HARDDISK, true);             // 允许 IDE 中断
    set_interrupt_mask(IRQ_HARDDISK2, true);            // 允许第二个 IDE 中断
    set_interrupt_mask(IRQ_CASCADE, true);              // 允许级联中断
}
