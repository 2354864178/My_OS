#include <onix/pci.h>
#include <onix/io.h>
#include <onix/printk.h>
#include <onix/memory.h>

#define PCI_CFG_ADDR 0xCF8  // PCI 配置地址端口
#define PCI_CFG_DATA 0xCFC  // PCI 配置数据端口

// 计算 PCI 配置地址
static u32 pci_config_addr(u8 bus, u8 dev, u8 func, u8 offset){
	// offset 必须 4 字节对齐
	return 0x80000000u |        // 启用位
		   ((u32)bus << 16) |   // 总线号
		   ((u32)dev << 11) |   // 设备号
		   ((u32)func << 8) |   // 功能号
		   (offset & 0xFC);     // 寄存器偏移（4 字节对齐）
}

// 读取 PCI 配置空间的 32 位值
u32 pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset){
	outl(PCI_CFG_ADDR, pci_config_addr(bus, dev, func, offset));
	return inl(PCI_CFG_DATA);
}

// 写 PCI 配置空间的 32 位值
void pci_config_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 value){
	outl(PCI_CFG_ADDR, pci_config_addr(bus, dev, func, offset));
	outl(PCI_CFG_DATA, value);
}

// 写 PCI 配置空间的 16 位值
void pci_config_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 value){
	u32 old = pci_config_read32(bus, dev, func, offset);
	u32 shift = (offset & 2) * 8;
	u32 mask = 0xFFFFu << shift;
	u32 nw = (old & ~mask) | ((u32)value << shift);
	pci_config_write32(bus, dev, func, offset, nw);
}

// 写 PCI 配置空间的 8 位值
void pci_config_write8(u8 bus, u8 dev, u8 func, u8 offset, u8 value){
	u32 old = pci_config_read32(bus, dev, func, offset);
	u32 shift = (offset & 3) * 8;
	u32 mask = 0xFFu << shift;
	u32 nw = (old & ~mask) | ((u32)value << shift);
	pci_config_write32(bus, dev, func, offset, nw);
}

// 读取 PCI 配置空间的 16 位值
u16 pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset){
	u32 value = pci_config_read32(bus, dev, func, offset);
	return (value >> ((offset & 2) * 8)) & 0xFFFFu;
}

// 读取 PCI 配置空间的 8 位值
u8 pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset){
	u32 value = pci_config_read32(bus, dev, func, offset);
	return (value >> ((offset & 3) * 8)) & 0xFFu;
}

// 检查 PCI 设备是否存在
static bool pci_present(u8 bus, u8 dev, u8 func){
	u16 vendor = pci_config_read16(bus, dev, func, 0x00);
	return vendor != 0xFFFFu;
}

// 检查是否为多功能设备
static bool pci_is_multifunction(u8 bus, u8 dev){
	u8 header_type = pci_config_read8(bus, dev, 0, 0x0E);
	return (header_type & 0x80u) != 0;
}

// 打印单个 PCI 设备信息
static void pci_print_one(u8 bus, u8 dev, u8 func){
	u16 vendor = pci_config_read16(bus, dev, func, 0x00);   // 供应商 ID
	u16 device = pci_config_read16(bus, dev, func, 0x02);   // 设备 ID

	u8 rev = pci_config_read8(bus, dev, func, 0x08);        // 修订 ID
	u8 prog_if = pci_config_read8(bus, dev, func, 0x09);    // 编程接口
	u8 subclass = pci_config_read8(bus, dev, func, 0x0A);   // 子类代码
	u8 class_code = pci_config_read8(bus, dev, func, 0x0B); // 类代码
	u8 header = pci_config_read8(bus, dev, func, 0x0E);     // 标头类型

	bool is_nvme = (class_code == 0x01) && (subclass == 0x08) && (prog_if == 0x02);

	printk("PCI %02x:%02x.%u vid:did %04x:%04x class %02x:%02x:%02x rev %02x hdr %02x%s\n",
		   bus, dev, func,
		   vendor, device,
		   class_code, subclass, prog_if,
		   rev, header,
		   is_nvme ? " [NVMe]" : "");
}

// 扫描并打印所有 PCI 设备信息
static void pci_scan_and_print(void){
	printk("\nPCI scan...\n");
    // 遍历所有可能的总线、设备和功能号
	for (u32 bus = 0; bus < 256; bus++)	{
		for (u32 dev = 0; dev < 32; dev++) {
			if (!pci_present((u8)bus, (u8)dev, 0)) continue; // 功能 0 不存在，跳过该设备

			u32 func_limit = pci_is_multifunction((u8)bus, (u8)dev) ? 8 : 1;    // 多功能设备检查功能数
			
            for (u32 func = 0; func < func_limit; func++){
				if (!pci_present((u8)bus, (u8)dev, (u8)func)) continue; // 功能不存在，跳过
				pci_print_one((u8)bus, (u8)dev, (u8)func);              // 打印该 PCI 设备信息
			}
		}
	}
	printk("PCI scan done.\n\n");
}

void pci_init(void){
	pci_scan_and_print();   // 扫描并打印所有 PCI 设备信息
}

