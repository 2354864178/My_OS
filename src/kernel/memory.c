#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/memory.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)            // 获取 addr 的页索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引
#define PAGE(idx) ((u32)idx << 12)             // 获取页索引 idx 对应的页开始的位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

typedef struct ards_t
{
    u64 base; // 内存基地址
    u64 size; // 内存长度
    u32 type; // 类型
} _packed ards_t;

static u32 memory_base = 0; // 可用内存基地址，应该等于 1M
static u32 memory_size = 0; // 可用内存大小
static u32 total_pages = 0; // 所有内存页数
static u32 free_pages = 0;  // 空闲内存页数

#define used_pages (total_pages - free_pages) // 已用页数

void memory_init(u32 magic, u32 addr)
{
    // LOGK("Received magic: 0x%p\n", magic);  // 打印接收的魔数
    u32 count;  
    ards_t *ptr;

    // 如果是 onix loader 进入的内核
    if (magic == ONIX_MAGIC){
        count = *(u32 *)addr;               // 从ARDS表起始地址+4字节，读取区域总数（addr首4字节是count）
        ards_t *ptr = (ards_t *)(addr + 4); // ptr指向第一个ARDS区域描述符

        for (size_t i = 0; i < count; i++, ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n", (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);

            // 筛选：只保留“可用区域”中最大的那个（内核需要连续大内存）
            if (ptr->type == ZONE_VALID && ptr->size > memory_size) {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    }
    else{
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    assert(memory_base == MEMORY_BASE); // 可用内存基地址必须是1MB（MEMORY_BASE=0x100000）
    assert((memory_size & 0xfff) == 0); // 可用内存大小必须按4KB对齐（分页管理要求，非对齐内存无法按页映射）

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);
}

static u32 start_page = 0;   // 可分配物理内存起始位置
static u8 *memory_map;       // 物理内存状态映射表：1 = 页已占用，0 = 页空闲
static u32 memory_map_pages; // 映射表本身占用的物理页数

// 初始化用于跟踪物理页占用状态的映射表，标记前 1M 内存及映射表自身占用的物理页为已占用，更新空闲物理页数并打印相关日志，为后续系统物理内存的分配与释放提供基础。
void memory_map_init()
{
    
    memory_map = (u8 *)memory_base; // 初始化物理内存数组（物理内存映射表）
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);    // 计算映射表本身需要占用多少个物理页
    LOGK("Memory map page count %d\n", memory_map_pages);       //  打印映射表占用的物理页数

    free_pages -= memory_map_pages; // 更新系统空闲物理页数， 映射表本身占用了memory_map_pages个物理页。

    memset((void *)memory_map, 0, memory_map_pages * PAGE_SIZE); // 将映射表占用的所有内存空间清零

    // 前 1M 的内存位置 以及 物理内存数组已占用的页，已被占用
    start_page = IDX(MEMORY_BASE) + memory_map_pages;   // 计算已占用物理页的总数量，作为后续标记占用的边界
    for (size_t i = 0; i < start_page; i++) 
    {
        memory_map[i] = 1;  // 标记所有已占用的物理页为1（占用状态）。
    }

    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);    // 打印系统总物理页数和当前空闲页数

    // // 初始化内核虚拟内存位图，需要 8 位对齐
    // u32 length = (IDX(KERNEL_RAMDISK_MEM) - IDX(MEMORY_BASE)) / 8;
    // bitmap_init(&kernel_map, (u8 *)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    // bitmap_scan(&kernel_map, memory_map_pages);
}

// 分配一页物理内存，从start_page开始查找系统中第一个空闲物理页，标记其为已占用、更新空闲页数并返回该页的物理地址。
static u32 get_page()
{
    for (size_t i = start_page; i < total_pages; i++)   
    {
        if (!memory_map[i]) // 如果物理内存没有占用
        {
            memory_map[i] = 1;      // 将找到的空闲页标记为已占用。
            assert(free_pages > 0);
            free_pages--;           // 更新系统空闲物理页数
            u32 page = PAGE(i);     // 将空闲页的索引i转换为对应的物理页基地址。
            LOGK("GET page 0x%p\n", page);
            return page;
        }
    }
    panic("Out of Memory!!!");      // 未找到空闲页时，触发内核错误
}

// 释放一页物理内存
static void put_page(u32 addr)
{
    ASSERT_PAGE(addr);      // 强制验证输入地址addr是 4KB 对齐的物理页基地址
    u32 idx = IDX(addr);    // 将要释放的页基地址转换为对应的物理页索引

    assert(idx >= start_page && idx < total_pages); // idx 大于 1M 并且 小于 总页面数

    assert(memory_map[idx] >= 1);   // 验证要释放的页是已占用状态且有有效引用，避免重复释放或释放空闲页。
    memory_map[idx]--;              // 将该页的引用计数减一
    
    if (!memory_map[idx]) free_pages++; // 当引用计数减至 0（页真正空闲）时，更新系统空闲页数

    assert(free_pages > 0 && free_pages < total_pages);
    LOGK("PUT page 0x%p\n", addr);
}

// void memory_test(){
//     u32 pages[10];
//     for(size_t i=0; i<10; i++) pages[i] = get_page();
//     for(size_t i=0; i<10; i++) put_page(pages[i]);
// }

// 得到 cr3 寄存器
u32 get_cr3(){
    asm volatile("movl %cr3, %eax\n");  // 直接将 mov eax, cr3，返回值在 eax 中
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde)
{
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

// 将 cr0 寄存器最高位 PG 置为 1，启用分页
static void enable_page()
{
    // 0b1000_0000_0000_0000_0000_0000_0000_0000
    // 0x80000000
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n");
}

// 初始化页表项
static void entry_init(page_entry_t *entry, u32 index)
{
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

#define KERNEL_PAGE_DIR 0x200000
#define KERNEL_PAGE_ENTRY 0x201000

// 初始化内存映射
void mapping_init()
{
    page_entry_t *pde = (page_entry_t *)KERNEL_PAGE_DIR;    // 初始化页目录表，PDE[0]指向页表
    memset(pde, 0, PAGE_SIZE);  // PDE的index=0x201 → 页表地址=0x201000

    entry_init(&pde[0], IDX(KERNEL_PAGE_ENTRY));

    page_entry_t *pte = (page_entry_t *)KERNEL_PAGE_ENTRY;  // 初始化页表，1024个PTE对应虚拟地址0~4MB
    page_entry_t *entry;
    memset(pte, 0, PAGE_SIZE);

    for (size_t tidx = 0; tidx < 1024; tidx++){
        entry = &pte[tidx];
        entry_init(entry, tidx);    // PTE的index=tidx → 物理页地址=tidx<<12
        memory_map[tidx] = 1;       // 设置物理内存数组，该页被占用
    }

    BMB;
    set_cr3((u32)pde);  // 设置 cr3 寄存器
    BMB;
    
    enable_page();      // 启用分页
}
