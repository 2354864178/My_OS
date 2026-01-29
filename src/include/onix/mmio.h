#ifndef ONIX_MMIO_H
#define ONIX_MMIO_H

#include <onix/types.h>

// 内存屏障，防止编译器重排序
static _inline void io_mb(void){
	asm volatile("" ::: "memory");
}

// 8 位宽度的 MMIO 读函数
static _inline u8 mmio_read8(uintptr_t addr){
	u8 v = *(volatile u8 *)addr;
	io_mb();                        // 内存屏障,确保读操作完成后再执行后续代码
	return v;
}

// 16 位宽度的 MMIO 读函数
static _inline u16 mmio_read16(uintptr_t addr){
	u16 v = *(volatile u16 *)addr;
	io_mb();
	return v;
}

// 32 位宽度的 MMIO 读函数
static _inline u32 mmio_read32(uintptr_t addr){
	u32 v = *(volatile u32 *)addr;
	io_mb();
	return v;
}

// 8 位宽度的 MMIO 写函数
static _inline void mmio_write8(uintptr_t addr, u8 value){
	io_mb();
	*(volatile u8 *)addr = value;
	io_mb();
}

// 16 位宽度的 MMIO 写函数
static _inline void mmio_write16(uintptr_t addr, u16 value){
	io_mb();
	*(volatile u16 *)addr = value;
	io_mb();
}

// 32 位宽度的 MMIO 写函数
static _inline void mmio_write32(uintptr_t addr, u32 value){
	io_mb();
	*(volatile u32 *)addr = value;
	io_mb();
}

#endif