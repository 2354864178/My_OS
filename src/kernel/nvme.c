#include <onix/nvme.h>
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
#include <onix/pci.h>
#include <onix/mmio.h>

// 分区类型
typedef enum PART_FS{ 
    PART_FS_FAT12 = 1,      // FAT12 
    PART_FS_EXTENDED = 5,   // 扩展分区
    PART_FS_MINIX = 0x80,   // Minix 
    PART_FS_LINUX = 0x83,   // Linux 
} PART_FS;

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// PCI class codes (用于识别 NVMe Controller)
#define PCI_CLASS_MASS_STORAGE 0x01 // 质量存储控制器
#define PCI_SUBCLASS_NVM       0x08 // 非易失性存储器控制器
#define PCI_PROGIF_NVME        0x02 // NVMe 编程接口

// NVMe 寄存器偏移
#define NVME_REG_CAP   0x0000       // 控制器能力寄存器
#define NVME_REG_VS    0x0008       // 版本寄存器
#define NVME_REG_CC    0x0014       // 控制器配置寄存器
#define NVME_REG_CSTS  0x001C       // 控制器状态寄存器
#define NVME_REG_AQA   0x0024       // 管理队列属性寄存器
#define NVME_REG_ASQ   0x0028       // 管理提交队列基址寄存器
#define NVME_REG_ACQ   0x0030       // 管理完成队列基址寄存器
#define NVME_REG_DBS   0x1000       // doorbell 寄存器起始偏移

#define NVME_ADMIN_CREATE_IOSQ 0x01 // 创建 IO 提交队列
#define NVME_ADMIN_CREATE_IOCQ 0x05 // 创建 IO 完成队列
#define NVME_ADMIN_IDENTIFY    0x06 // 识别命令

#define NVME_CMD_WRITE         0x01 // 写命令
#define NVME_CMD_READ          0x02 // 读命令

// NVMe 命令结构体
typedef struct nvme_cmd_t{
    u8  opc;            // 操作码
    u8  fuse_psdt;      // FUSE 和 PSDT
    u16 cid;            // 命令标识符
    u32 nsid;           // 命名空间标识符
    u64 rsvd2;          // 保留
    u64 mptr;           // 元数据指针
    u64 prp1;           // 物理区域页 1
    u64 prp2;           // 物理区域页 2
    u32 cdw10;          // 命令特定字段
    u32 cdw11;          // 命令特定字段
    u32 cdw12;
    u32 cdw13;
    u32 cdw14;
    u32 cdw15;
} _packed nvme_cmd_t;

// NVMe 完成队列条目结构体
typedef struct nvme_cpl_t{
    u32 cdw0;       // 命令特定字段
    u32 rsvd1;      // 保留
    u16 sqhd;       // 提交队列头指针
    u16 sqid;       // 提交队列标识符
    u16 cid;        // 命令标识符
    u16 status;     // 状态字段
} _packed nvme_cpl_t;

static nvme_ctrl_t nvme_ctrls[NVME_CTRL_NR];    // NVMe 控制器数组

// 读取/写入 NVMe MMIO 寄存器
static _inline u32 nvme_read32(nvme_ctrl_t *ctrl, u32 off){
    return mmio_read32((uintptr_t)(ctrl->mmio_base + off));
}

static _inline void nvme_write32(nvme_ctrl_t *ctrl, u32 off, u32 value){
    mmio_write32((uintptr_t)(ctrl->mmio_base + off), value);
}

static _inline u64 nvme_read64(nvme_ctrl_t *ctrl, u32 off){
    u32 lo = nvme_read32(ctrl, off);
    u32 hi = nvme_read32(ctrl, off + 4);
    return ((u64)hi << 32) | lo;
}

static _inline void nvme_write64(nvme_ctrl_t *ctrl, u32 off, u64 value){
    nvme_write32(ctrl, off, (u32)(value & 0xFFFFFFFFu));
    nvme_write32(ctrl, off + 4, (u32)((value >> 32) & 0xFFFFFFFFu));
}

// 计算 doorbell 偏移
static _inline u32 nvme_db_off(nvme_ctrl_t *ctrl, u16 qid, bool cq){
    // doorbell: 0x1000 + (2*qid + (cq?1:0)) * stride
    return NVME_REG_DBS + (u32)(2 * qid + (cq ? 1 : 0)) * ctrl->db_stride;
}

// 映射 NVMe MMIO 寄存器
static void nvme_map_mmio(u32 base, u32 size){
    // 采用物理=虚拟的映射方式（与 LAPIC/IOAPIC 一致）
    for (u32 off = 0; off < size; off += PAGE_SIZE){
        map_page_fixed(base + off, base + off, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    }
}

// 等待 NVMe 控制器就绪状态
static int nvme_wait_ready(nvme_ctrl_t *ctrl, bool ready){
    for (u32 i = 0; i < 1000000; i++) {
        u32 csts = nvme_read32(ctrl, NVME_REG_CSTS);
        bool rdy = (csts & 1u) != 0;
        if (rdy == ready) return 0;
    }
    return EOF;
}

// 获取下一个命令标识符
static u16 nvme_next_cid(nvme_ctrl_t *ctrl) {
    u16 cid = ctrl->next_cid++;                     // 分配当前 cid 并自增
    if (ctrl->next_cid == 0) ctrl->next_cid = 1;    // 避免 cid 为 0
    return cid;
}

// 提交 Admin 命令并等待完成
static int nvme_admin_submit(nvme_ctrl_t *ctrl, nvme_cmd_t *cmd) {
    nvme_cmd_t *sq = (nvme_cmd_t *)ctrl->admin_sq;  // 提交队列
    nvme_cpl_t *cq = (nvme_cpl_t *)ctrl->admin_cq;  // 完成队列

    u16 tail = ctrl->admin_sq_tail;     // 获取当前 tail
    sq[tail] = *cmd;                    // 写入命令
    ctrl->admin_sq_tail = (tail + 1) % NVME_ADMIN_Q_DEPTH;  // 更新 tail
    nvme_write32(ctrl, nvme_db_off(ctrl, 0, false), ctrl->admin_sq_tail);   // 更新 doorbell

    // 轮询完成队列
    while (true) {
        nvme_cpl_t *cpl = &cq[ctrl->admin_cq_head];
        u16 status = cpl->status;
        u16 phase = status & 1u;
        if (phase != ctrl->admin_cq_phase) continue;

        // 匹配 CID（当前实现串行提交，也做基本校验）
        if (cpl->cid != cmd->cid) {
            LOGK("nvme admin cpl cid mismatch %u != %u\n", cpl->cid, cmd->cid);
        }

        u16 sc = (status >> 1) & 0xFFu;     // 提取状态码
        u16 sct = (status >> 9) & 0x7u;     // 提取状态码类型

        // 更新 CQ head/phase
        ctrl->admin_cq_head = (ctrl->admin_cq_head + 1) % NVME_ADMIN_Q_DEPTH;   // 更新完成队列头
        if (ctrl->admin_cq_head == 0) ctrl->admin_cq_phase ^= 1;                // 反转相位位
        nvme_write32(ctrl, nvme_db_off(ctrl, 0, true), ctrl->admin_cq_head);    // 更新 doorbell

        if (sc || sct){ 
            LOGK("nvme admin cmd failed sct %u sc %u\n", sct, sc);
            return EOF;
        }
        return 0;
    }
}

// 提交 IO 命令并等待完成
static int nvme_io_submit(nvme_ctrl_t *ctrl, nvme_cmd_t *cmd) {
    nvme_cmd_t *sq = (nvme_cmd_t *)ctrl->io_sq; // 提交队列
    nvme_cpl_t *cq = (nvme_cpl_t *)ctrl->io_cq; // 完成队列

    u16 tail = ctrl->io_sq_tail;    // 获取当前 tail
    sq[tail] = *cmd;                // 写入命令
    ctrl->io_sq_tail = (tail + 1) % NVME_IO_Q_DEPTH;    // 更新 tail
    nvme_write32(ctrl, nvme_db_off(ctrl, 1, false), ctrl->io_sq_tail);

    while (true) {
        nvme_cpl_t *cpl = &cq[ctrl->io_cq_head];    // 获取当前完成队列头
        u16 status = cpl->status;                   // 读取状态字段
        u16 phase = status & 1u;                    // 新的完成条目相位位
        if (phase != ctrl->io_cq_phase) continue;   // 新的完成条目未到达

        u16 sc = (status >> 1) & 0xFFu;     // 提取状态码
        u16 sct = (status >> 9) & 0x7u;     // 提取状态码类型

        ctrl->io_cq_head = (ctrl->io_cq_head + 1) % NVME_IO_Q_DEPTH;        // 更新完成队列头
        if (ctrl->io_cq_head == 0) ctrl->io_cq_phase ^= 1;                  // 切换相位位
        nvme_write32(ctrl, nvme_db_off(ctrl, 1, true), ctrl->io_cq_head);   // 更新 doorbell

        if (sc || sct) {
            LOGK("nvme io cmd failed sct %u sc %u\n", sct, sc); // 错误处理
            return EOF;
        }
        return 0;
    }
}

// 识别 NVMe 磁盘信息
static int nvme_identify(nvme_ctrl_t *ctrl, u32 nsid, u32 cns, void *buf) {
    memset(buf, 0, PAGE_SIZE);      // 清空缓冲区
    nvme_cmd_t cmd;                 // 构造命令
    memset(&cmd, 0, sizeof(cmd));   // 清空命令结构体
    cmd.opc = NVME_ADMIN_IDENTIFY;  // 识别命令
    cmd.cid = nvme_next_cid(ctrl);  // 获取命令标识符
    cmd.nsid = nsid;                // 命名空间 ID
    cmd.prp1 = (u32)buf;            // 32 位内核：物理=虚拟（alloc_kpage）
    cmd.cdw10 = cns;                // 命令特定字段 CNS
    return nvme_admin_submit(ctrl, &cmd);   // 提交命令
}

// 创建 IO 队列
static int nvme_create_io_queues(nvme_ctrl_t *ctrl) {
    // 分配 IO SQ/CQ（物理连续）
    ctrl->io_cq = (void *)alloc_kpage(1);   // 分配一页给完成队列
    ctrl->io_sq = (void *)alloc_kpage(1);   // 分配一页给提交队列
    memset(ctrl->io_cq, 0, PAGE_SIZE);  
    memset(ctrl->io_sq, 0, PAGE_SIZE);
    ctrl->io_sq_tail = 0;                   // 初始化提交队列尾指针
    ctrl->io_cq_head = 0;                   // 初始化完成队列头指针
    ctrl->io_cq_phase = 1;                  // 初始化完成队列相位位

    // Create IO Completion Queue (qid=1)
    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = NVME_ADMIN_CREATE_IOCQ;
    cmd.cid = nvme_next_cid(ctrl);
    cmd.prp1 = (u32)ctrl->io_cq;
    // cdw10: QID[15:0] | QSIZE[31:16]
    cmd.cdw10 = (1u & 0xFFFFu) | ((u32)(NVME_IO_Q_DEPTH - 1) << 16);
    // cdw11: PC=1(bit0), IEN=0(bit1), IV=0
    cmd.cdw11 = 1u;
    if (nvme_admin_submit(ctrl, &cmd) != 0) return EOF;

    // Create IO Submission Queue (qid=1, cqid=1)
    memset(&cmd, 0, sizeof(cmd));
    cmd.opc = NVME_ADMIN_CREATE_IOSQ;
    cmd.cid = nvme_next_cid(ctrl);
    cmd.prp1 = (u32)ctrl->io_sq;
    cmd.cdw10 = (1u & 0xFFFFu) | ((u32)(NVME_IO_Q_DEPTH - 1) << 16);
    // cdw11: CQID[15:0] | QFLAGS[31:16]，QFLAGS.PC 在 bit16
    cmd.cdw11 = 1u | (1u << 16);
    return nvme_admin_submit(ctrl, &cmd);
}

// 初始化 NVMe 控制器
static int nvme_ctrl_init_one(nvme_ctrl_t *ctrl, u32 mmio_base) {
    memset(ctrl, 0, sizeof(*ctrl));
    raw_mutex_init(&ctrl->lock);
    ctrl->mmio_base = mmio_base;
    ctrl->next_cid = 1;

    // 映射一段寄存器窗口
    nvme_map_mmio(mmio_base, 0x4000);

    u64 cap = nvme_read64(ctrl, NVME_REG_CAP);  // 读取能力寄存器
    u32 vs = nvme_read32(ctrl, NVME_REG_VS);    // 读取版本寄存器
    u32 dstrd = (u32)((cap >> 32) & 0xFu);      // doorbell stride
    ctrl->db_stride = (1u << dstrd) * 4;        // 计算 doorbell stride 字节数
    LOGK("nvme mmio 0x%p cap 0x%p vs 0x%p dstrd %u\n", ctrl->mmio_base, (u32)cap, vs, dstrd);

    // 复位控制器
    nvme_write32(ctrl, NVME_REG_CC, 0);         // 禁用控制器
    if (nvme_wait_ready(ctrl, false) != 0) {
        LOGK("nvme disable timeout\n");
        return EOF;
    }

    // 设置 Admin 提交/完成队列
    ctrl->admin_cq = (void *)alloc_kpage(1);    // 分配一页给完成队列
    ctrl->admin_sq = (void *)alloc_kpage(1);    // 分配一页给提交队列
    memset(ctrl->admin_cq, 0, PAGE_SIZE);   // 清空完成队列内存
    memset(ctrl->admin_sq, 0, PAGE_SIZE);   // 清空提交队列内存
    ctrl->admin_sq_tail = 0;                // 提交队列尾指针
    ctrl->admin_cq_head = 0;                // 完成队列头指针
    ctrl->admin_cq_phase = 1;               // 完成队列相位位

    // 设置 Admin 队列寄存器
    u32 aqa = ((NVME_ADMIN_Q_DEPTH - 1) << 16) | (NVME_ADMIN_Q_DEPTH - 1);  // 设置队列深度
    nvme_write32(ctrl, NVME_REG_AQA, aqa);  // 写入队列属性寄存器
    nvme_write64(ctrl, NVME_REG_ASQ, (u32)ctrl->admin_sq);  // 提交队列基址
    nvme_write64(ctrl, NVME_REG_ACQ, (u32)ctrl->admin_cq);  // 完成队列基址

    // 启用控制器
    u32 cc = 0;          // 构造控制器配置值
    cc |= 1u;            // EN
    cc |= (0u << 7);     // MPS
    cc |= (6u << 16);    // IOSQES
    cc |= (4u << 20);    // IOCQES
    nvme_write32(ctrl, NVME_REG_CC, cc);  // 写入控制器配置寄存器
    // 等待就绪
    if(nvme_wait_ready(ctrl, true) != 0) { 
        LOGK("nvme enable timeout\n");
        return EOF;
    }

    if(nvme_create_io_queues(ctrl) != 0){
        LOGK("nvme create io queues failed\n");
        return EOF;
    }

    return 0;
}

// 查找第 nth 个 NVMe 设备的 MMIO 基址
static int nvme_find_nth_mmio(u32 nth, u32 *mmio_out){
    u32 found = 0;
    for (u32 bus = 0; bus < 256; bus++){
        for (u32 dev = 0; dev < 32; dev++){
            // func0 不存在直接跳过
            if (pci_config_read16((u8)bus, (u8)dev, 0, 0x00) == 0xFFFFu) continue;

            u8 header_type = pci_config_read8((u8)bus, (u8)dev, 0, 0x0E);
            u32 func_limit = (header_type & 0x80u) ? 8 : 1;

            for (u32 func = 0; func < func_limit; func++){
                if (pci_config_read16((u8)bus, (u8)dev, (u8)func, 0x00) == 0xFFFFu) continue;

                u8 class_code = pci_config_read8((u8)bus, (u8)dev, (u8)func, 0x0B);
                u8 subclass = pci_config_read8((u8)bus, (u8)dev, (u8)func, 0x0A);
                u8 prog_if = pci_config_read8((u8)bus, (u8)dev, (u8)func, 0x09);

                if (class_code != PCI_CLASS_MASS_STORAGE) continue;
                if (subclass != PCI_SUBCLASS_NVM) continue;
                if (prog_if != PCI_PROGIF_NVME) continue;

                u32 bar0 = pci_config_read32((u8)bus, (u8)dev, (u8)func, 0x10);
                u32 bar1 = pci_config_read32((u8)bus, (u8)dev, (u8)func, 0x14);
                u32 mmio_lo = bar0 & ~0xFu;
                u32 mmio_hi = bar1;

                // 打开 Memory Space + Bus Master
                u16 cmd = pci_config_read16((u8)bus, (u8)dev, (u8)func, 0x04);
                cmd |= (1u << 1); // MEM
                cmd |= (1u << 2); // BUS MASTER
                pci_config_write16((u8)bus, (u8)dev, (u8)func, 0x04, cmd);

                if (mmio_hi){
                    panic("nvme %02x:%02x.%u mmio >4GiB unsupported\n", bus, dev, func);
                }

                if (found == nth) {
                    *mmio_out = mmio_lo;
                    return 0;
                }
                found++;
            }
        }
    }
    return EOF;
}

// 识别 NVMe 磁盘信息
static int nvme_disk_identify(nvme_disk_t *disk) {
    u8 *buf = (u8 *)alloc_kpage(1);     // 分配一页缓冲区
    nvme_ctrl_t *ctrl = disk->ctrl;     // 获取控制器

    // 发送识别命令
    if (nvme_identify(ctrl, disk->nsid, 0, buf) != 0){
        free_kpage((u32)buf, 1);
        return EOF;
    }

    // NSZE at offset 0
    u32 nsze_lo = *(u32 *)(buf + 0);    // 命名空间大小低 32 位
    u32 nsze_hi = *(u32 *)(buf + 4);    // 命名空间大小高 32 位
    if (nsze_hi) {                      // 仅支持 <=32 位大小
        LOGK("nvme nsze >32-bit unsupported\n");
        free_kpage((u32)buf, 1);
        return EOF;
    }

    u8 flbas = *(u8 *)(buf + 0x1A);     // LBA 格式支持位图
    u8 fmt = flbas & 0x0Fu;             // 当前 LBA 格式索引
    u8 *lbaf = buf + 0x80 + fmt * 4;    // LBA 格式描述符
    u8 lbads = lbaf[2];                 // LBA 数据大小 (2^LBADS 字节)
    disk->lba_size = 1u << lbads;       // 计算 LBA 大小

    if (disk->lba_size != SECTOR_SIZE){ // 仅支持 512 字节扇区大小
        LOGK("nvme lba size %u unsupported (need %u)\n", disk->lba_size, SECTOR_SIZE);
        free_kpage((u32)buf, 1);
        return EOF;
    }

    disk->total_sectors = nsze_lo;      // 设置总扇区数
    LOGK("nvme nsid %u sectors %u lba_size %u\n", disk->nsid, disk->total_sectors, disk->lba_size);

    free_kpage((u32)buf, 1);           // 释放缓冲区
    return 0;
}

// 读写 NVMe 磁盘
static int nvme_rw(nvme_disk_t *disk, void *buffer, u8 count, idx_t lba, bool write){
    assert(count > 0);                      // 必须读写至少一个扇区
    assert(disk->lba_size == SECTOR_SIZE);  // 仅支持 512 字节扇区大小

    // 只支持单页内传输（<=4K）并且 buffer 物理连续（alloc_kpage 满足）
    if ((u32)count * SECTOR_SIZE > PAGE_SIZE) {
        panic("nvme rw too large: %u\n", count); // 超过单页大小
    }

    nvme_ctrl_t *ctrl = disk->ctrl; // 获取控制器
    raw_mutex_lock(&ctrl->lock);    // 加锁

    // 使用 bounce buffer 避免对上层 buffer 物理连续/映射方式的假设
    void *bounce = (void *)alloc_kpage(1);

    // 写入时拷贝数据到 bounce buffer
    if (write) memcpy(bounce, buffer, (u32)count * SECTOR_SIZE);    

    nvme_cmd_t cmd;                 // 构造命令
    memset(&cmd, 0, sizeof(cmd));   // 清空命令结构体
    cmd.opc = write ? NVME_CMD_WRITE : NVME_CMD_READ;   // 读写命令
    cmd.cid = nvme_next_cid(ctrl);  // 获取命令标识符
    cmd.nsid = disk->nsid;          // 命名空间 ID
    cmd.prp1 = (u32)bounce;         // 32 位内核：物理=虚拟（alloc_kpage）
    cmd.cdw10 = (u32)lba;           // 起始 LBA 低 32 位
    cmd.cdw11 = 0;                  // 起始 LBA 高 32 位
    cmd.cdw12 = (u32)(count - 1);   // 传输扇区数（0 表示 1 个扇区）

    int ret = nvme_io_submit(ctrl, &cmd);   // 提交命令

    // 读取时拷贝数据到上层 buffer
    if (!write && ret == 0) memcpy(buffer, bounce, (u32)count * SECTOR_SIZE);  

    free_kpage((u32)bounce, 1);     // 释放 bounce buffer
    raw_mutex_unlock(&ctrl->lock);  // 解锁
    return ret;
}

// NVMe 磁盘读写
int nvme_pio_read(nvme_disk_t *disk, void *buffer, u8 count, idx_t lba) {
    return nvme_rw(disk, buffer, count, lba, false);
}

int nvme_pio_write(nvme_disk_t *disk, void *buffer, u8 count, idx_t lba) {
    return nvme_rw(disk, buffer, count, lba, true);
}

// NVMe 磁盘 IOCTL 操作
int nvme_pio_ioctl(nvme_disk_t *disk, int cmd, void *args, int flags) {
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:  // 起始扇区
        return 0;
    case DEV_CMD_SECTOR_COUNT:  // 扇区总数
        return disk->total_sectors;
    default:
        panic("nvme_pio_ioctl: unsupported cmd %d\n", cmd);
        break;
    }
}

// NVMe 分区读写
int nvme_pio_part_read(nvme_part_t *part, void *buffer, u8 count, idx_t lba) {
    return nvme_pio_read(part->disk, buffer, count, part->start + lba);
}

int nvme_pio_part_write(nvme_part_t *part, void *buffer, u8 count, idx_t lba){
    return nvme_pio_write(part->disk, buffer, count, part->start + lba);
}

// NVMe 分区 IOCTL 操作
int nvme_pio_part_ioctl(nvme_part_t *part, int cmd, void *args, int flags) {
    switch (cmd)
    {
    case DEV_CMD_SECTOR_START:
        return part->start;    
    case DEV_CMD_SECTOR_COUNT:
        return part->count;
    default:
        panic("nvme_pio_part_ioctl: unsupported cmd %d\n", cmd);
        break;
    }
}

static void nvme_part_init(nvme_disk_t *disk, u16 *buf)
{
    if (!disk->total_sectors) return;
    // MBR 在 LBA0
    if (nvme_pio_read(disk, buf, 1, 0) != 0) return;

    boot_sector_t *bs = (boot_sector_t *)buf;
    for (size_t i = 0; i < NVME_PART_NR; i++){
        part_entry_t *entry = &bs->entry[i];    // 分区表项
        nvme_part_t *part = &disk->disk[i];     // 分区结构体
        if (!entry->system) continue;           // 空分区跳过

        sprintf(part->name, "%s%d", disk->name, i + 1);
        part->disk = disk;
        part->system = entry->system;
        part->start = entry->start;
        part->count = entry->count;
    }
}

static void nvme_install(nvme_ctrl_t *ctrl){
    for (size_t didx = 0; didx < NVME_DISK_NR; didx++){
        nvme_disk_t *disk = &ctrl->disks[didx];
        if (!disk->total_sectors) continue;
        dev_t dev = device_install(DEV_BLOCK, DEV_NVME_DISK, disk, disk->name, 0,
                                   nvme_pio_ioctl, nvme_pio_read, nvme_pio_write);
        for (size_t pidx = 0; pidx < NVME_PART_NR; pidx++){
            nvme_part_t *part = &disk->disk[pidx];
            if (!part->count) continue;
            device_install(DEV_BLOCK, DEV_NVME_PART, part, part->name, dev,
                           nvme_pio_part_ioctl, nvme_pio_part_read, nvme_pio_part_write);
        }
    }
}

// 初始化 NVMe 子系统
void nvme_init(void) {
    u16 *buf = (u16 *)alloc_kpage(1);

    for (u32 i = 0; i < NVME_CTRL_NR; i++) {
        u32 mmio;
        if (nvme_find_nth_mmio(i, &mmio) != 0) break;

        nvme_ctrl_t *ctrl = &nvme_ctrls[i];
        sprintf(ctrl->name, "nvme%u", i);
        if (nvme_ctrl_init_one(ctrl, mmio) != 0) continue;

        nvme_disk_t *disk = &ctrl->disks[0];
        sprintf(disk->name, "nv%u", i);
        disk->ctrl = ctrl;
        disk->nsid = 1;

        if (nvme_disk_identify(disk) != 0) continue;
        nvme_part_init(disk, buf);
        nvme_install(ctrl);
    }

    free_kpage((u32)buf, 1);
}