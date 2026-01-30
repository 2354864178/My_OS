
#ifndef ONIX_PCI_H
#define ONIX_PCI_H

#include <onix/types.h>
#include <onix/mutex.h>

// 初始化/枚举 PCI 设备并打印信息
void pci_init(void);

// 读取 PCI 配置空间（机制 #1：0xCF8/0xCFC）
u32 pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset);
u16 pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset);
u8  pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset);

// 写 PCI 配置空间（机制 #1：0xCF8/0xCFC）
void pci_config_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 value);
void pci_config_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 value);
void pci_config_write8(u8 bus, u8 dev, u8 func, u8 offset, u8 value);

#endif

