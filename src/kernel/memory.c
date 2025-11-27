#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/memory.h>
#include <onix/bitmap.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)            // 获取 addr 的页索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引，虚拟地址高10位，pde
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引，虚拟地址中间10位，pte
#define PAGE(idx) ((u32)idx << 12)             // 获取页索引 idx 对应的页开始的位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

#define KERNEL_PAGE_DIR 0x1000      // 内核页目录索引

static u32 KERNEL_PAGE_TABLE[] = {  // 内核页表索引
    0x2000,
    0x3000,
};

#define KERNEL_MAP_BITS 0x4000      // 内核内存位图缓冲区起始地址
#define KERNEL_MEMORY_SIZE (0x100000 * sizeof(KERNEL_PAGE_TABLE)) // 内核内存大小

bitmap_t kernel_map; // 内核内存位图

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

    // 验证加载器合法性（魔数匹配）
    if (magic == ONIX_MAGIC){
        // 读取ARDS区域总数和描述符（addr指向ards_count变量）
        count = *(u32 *)addr;               // 从ARDS表起始地址+4字节，读取区域总数（addr首4字节是count）
        ards_t *ptr = (ards_t *)(addr + 4); // ptr指向第一个ARDS区域描述符

        // 遍历ARDS表，筛选最大的可用内存区域
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

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);  // 总页数=1MB以上页数+1MB以下页数
    free_pages = IDX(memory_size);                      // 空闲页数=可用内存页数（1MB以上）

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed\n",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
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

    // 初始化内核虚拟内存位图，需要 8 位对齐
    u32 length = (IDX(KERNEL_MEMORY_SIZE) - IDX(MEMORY_BASE)) / 8;  // 计算内核内存位图长度，单位字节
    bitmap_init(&kernel_map, (u8 *)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE)); // 初始化内核内存位图结构体
    bitmap_scan(&kernel_map, memory_map_pages); // 将内核内存位图中前 memory_map_pages 位标记为已用，表示这些页已被映射表占用。
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
static _inline void enable_page()
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

// 初始化内存映射
void mapping_init()
{
    page_entry_t *pde = (page_entry_t *)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;

    for (idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); didx++)
    {
        page_entry_t *pte = (page_entry_t *)KERNEL_PAGE_TABLE[didx];    // 拿到当前页表的物理地址，强制转换为页表项指针
        memset(pte, 0, PAGE_SIZE);  // 清空页表

        // 初始化对应的页目录项（PDE），让 PDE 指向当前页表的物理地址
        page_entry_t *dentry = &pde[didx];  
        entry_init(dentry, IDX((u32)pte));  // 将页表物理地址转为页索引，存入 PDE 的 index 字段

        // 初始化当前页表的所有页表项（PTE）
        for (idx_t tidx = 0; tidx < 1024; tidx++, index++)
        {
            
            if (index == 0) continue;   // 关键：跳过第0页映射，为造成空指针访问，缺页异常，便于排错

            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry, index);  // 核心映射规则：虚拟页索引 = 物理页索引（恒等映射变种）
            memory_map[index] = 1;      // 衔接物理内存映射表：标记该物理页为占用
        }
    }

    // 将最后一个页表指向页目录自己，方便修改
    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    set_cr3((u32)pde);  // 设置 cr3 寄存器
    // BMB;
    enable_page();      // 分页有效
}

// 获取页目录
static page_entry_t *get_pde(){
    return (page_entry_t *)(0xfffff000);    // // 自映射对应的虚拟地址
}

// 获取虚拟地址 vaddr 对应的页表
static page_entry_t *get_pte(u32 vaddr){
    return (page_entry_t *) (0xffc00000 | (DIDX(vaddr) << 12)); // 0xffc00000 是 DIDX=1022 对应的虚拟地址基址（1022<<22=0xff800000？需精准计算：1022<<22=0xff800000，加上页表索引偏移）
}

// 刷新虚拟地址 vaddr 的 块表 TLB
void flush_tlb(u32 vaddr){
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

// 从位图中扫描 count 个连续的页
static u32 scan_page(bitmap_t *map, u32 count)
{
    assert(count > 0);
    int32 index = bitmap_scan(map, count);          // 从位图中找到 count 个连续的空闲位（0），返回起始位索引

    if (index == EOF) panic("Scan page fail!!!");   // 未找到足够连续空闲页时，触发内核错误

    u32 addr = PAGE(index);     // 将起始位索引转换为对应的页基地址
    LOGK("Scan page 0x%p count %d\n", addr, count); 
    return addr; 
}

// 与 scan_page 相对，重置相应的页
static void reset_page(bitmap_t *map, u32 addr, u32 count)
{
    ASSERT_PAGE(addr);
    assert(count > 0);
    u32 index = IDX(addr);   // 将页基地址转换为对应的页索引

    for (size_t i = 0; i < count; i++) {
        assert(bitmap_test(map, index + i));    // 验证要释放的页是已占用状态，避免重复释放或释放空闲页。
        bitmap_set(map, index + i, 0);          // 将对应位图位置0，标记为未占用
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count)
{
    assert(count > 0); 
    u32 vaddr = scan_page(&kernel_map, count); // 从内核内存位图中扫描 count 个连续的空闲页，返回起始页基地址
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count); 
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count)
{
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);      // 重置内核内存位图中对应的 count 个页，标记为未占用
    LOGK("FREE  kernel pages 0x%p count %d\n", vaddr, count);
}

void memory_test(){
    u32 *pages = (u32 *)(0x200000);
    u32 count = 0x6fe;
    for (size_t i = 0; i < count; i++) {
        pages[i] = alloc_kpage(1);
        LOGK("0x%x\n", i);
    }
    for (size_t i = 0; i < count; i++){
        free_kpage(pages[i], 1);
    }
}
