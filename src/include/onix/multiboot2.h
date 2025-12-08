#ifndef ONIX_MULTIBOOT2
#define ONIX_MULTIBOOT2

#include <onix/types.h>

#define MULTIBOOT2_MAGIC 0x36d76289 // 多重引导2魔数

#define MULTIBOOT_TAG_TYPE_END 0    // 多重引导2结束标签类型
#define MULTIBOOT_TAG_TYPE_MMAP 6   // 多重引导2内存映射标签类型

#define MULTIBOOT_MEMORY_AVAILABLE 1        // 可用内存
#define MULTIBOOT_MEMORY_RESERVED 2         // 保留内存
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3 // ACPI可回收内存 
#define MULTIBOOT_MEMORY_NVS 4              // ACPI NVS内存
#define MULTIBOOT_MEMORY_BADRAM 5           // 坏内存

// 多重引导2标签结构
typedef struct multi_tag_t{
    u32 type;   // 标签类型
    u32 size;   // 标签大小
} multi_tag_t;

// 多重引导2内存映射标签结构
typedef struct multi_mmap_entry_t{
    u64 addr;   // 起始地址
    u64 len;    // 长度
    u32 type;   // 内存类型
    u32 zero;   // 保留字段
} multi_mmap_entry_t;

// 多重引导2内存映射标签头结构
typedef struct multi_tag_mmap_t{
    u32 type;           // 标签类型
    u32 size;           // 标签大小
    u32 entry_size;     // 每个内存映射条目的大小
    u32 entry_version;  // 内存映射条目版本
    multi_mmap_entry_t entries[0];  // 内存映射条目数组
} multi_tag_mmap_t;

#endif
