#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/memory.h>
#include <onix/bitmap.h>
#include <onix/multiboot2.h>
#include <onix/task.h>
#include <onix/string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)            // 获取 addr 的页索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引，虚拟地址高10位，pde
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引，虚拟地址中间10位，pte
#define PAGE(idx) ((u32)idx << 12)             // 获取页索引 idx 对应的页开始的位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

#define KERNEL_MAP_BITS 0x4000      // 内核内存位图缓冲区起始地址

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
    u32 count = 0;  

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
    else if(magic == MULTIBOOT2_MAGIC){
        u32 total_size = *(u32 *)addr;                  // 多重引导2信息总大小
        multi_tag_t *tag = (multi_tag_t *)(addr + 8);   // 第一个标签起始地址

        LOGK("Multiboot2 total size 0x%p\n", total_size);   // 打印多重引导2信息总大小
        while (tag->type != MULTIBOOT_TAG_TYPE_END) {       // 遍历标签列表，直到遇到结束标签
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)  break;   // 找到内存映射标签，跳出循环
            tag = (multi_tag_t *)((u32)tag + ((tag->size + 7) & ~7)); // 下一个标签，8字节对齐
        }
        multi_tag_mmap_t *mtag = (multi_tag_mmap_t *)tag;   // 强制转换为内存映射标签结构体指针
        multi_mmap_entry_t *entry = mtag->entries;          // 第一个内存映射条目
        while((u32)entry < (u32)tag + tag->size){           // 遍历所有内存映射条目
            LOGK("Memory base 0x%x size 0x%p type %d\n", (u32)entry->addr, (u32)entry->len, (u32)entry->type);

            count++;
            // 筛选：只保留“可用区域”中最大的那个（内核需要连续大内存）
            if (entry->type == ZONE_VALID && entry->len > memory_size) {
                memory_base = (u32)entry->addr; // 可用内存基地址
                memory_size = (u32)entry->len;  // 可用内存大小
            }
            entry = (multi_mmap_entry_t *)((u32)entry + mtag->entry_size); // 下一个内存映射条目
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
    for (size_t i = 0; i < start_page; i++) {
        memory_map[i] = 1;  // 标记所有已占用的物理页为1（占用状态）。
    }

    LOGK("Total pages %d free pages %d\n\n", total_pages, free_pages);    // 打印系统总物理页数和当前空闲页数

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

u32 get_cr2(){
    asm volatile("movl %cr2, %eax\n");  // 直接将 mov eax, cr2，返回值在 eax 中
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

// 初始化页表项，带标志位
static void entry_init_flags(page_entry_t *entry, u32 index, u32 flags){
    *(u32 *)entry = 0;          // 清空页表项
    entry->present = (flags & PAGE_PRESENT) ? 1 : 0;    // 设置存在位
    entry->write = (flags & PAGE_WRITE) ? 1 : 0;        // 设置可写位
    entry->user = (flags & PAGE_USER) ? 1 : 0;          // 设置用户位
    entry->pwt = (flags & PAGE_PWT) ? 1 : 0;            // 设置页写通过位
    entry->pcd = (flags & PAGE_PCD) ? 1 : 0;            // 设置页缓存禁用位
    entry->global = (flags & PAGE_GLOBAL) ? 1 : 0;      // 设置全局页位
    entry->index = index;                               // 设置页索引

}

// 初始化内存映射
void mapping_init()
{
    page_entry_t *pde = (page_entry_t *)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;

    // 初始化内核页目录和页表，实现虚拟地址到物理地址的恒等映射
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
    map_page_fixed(0xFEE00000, 0xFEE00000, PAGE_PRESENT | PAGE_WRITE | PAGE_PCD); // 映射本地 APIC 寄存器
}

// 获取页目录
static page_entry_t *get_pde(){
    return (page_entry_t *)(0xfffff000);    // 自映射对应的虚拟地址
}

// 获取虚拟地址 vaddr 对应的页表
static page_entry_t *get_pte(u32 vaddr, bool create){
    page_entry_t *pde = get_pde();      // 找到页目录
    u32 idx = DIDX(vaddr);              // 找到页目录的索引，即页表的入口
    page_entry_t *entry = &pde[idx];    // 找到页表的入口位置
    assert(create || (!create && entry->present));                      // 判断要么页表存在，要么不存在但create为true
    page_entry_t *table = (page_entry_t *)(PDE_MASK | (idx << 12));     // 找到可以修改的页表

    // 如果页表不存在且需要创建
    if (!entry->present) {
        LOGK("Get and create page table entry for 0x%p\n", vaddr);
        u32 page = get_page();          // 获取一页物理内存
        entry_init(entry, IDX(page));   // 初始化并链接到entry上
        memset(table, 0, PAGE_SIZE);    // 清空新页表
    }
    return table;    // 返回该虚拟地址对应的页表
}

// 复制一页内存，返回新页的物理地址
static u32 copy_page(void *page) {
    u32 paddr = get_page();     // 获取一个8M以上的物理页，物理地址存储在 paddr 中。
    // 获取虚拟地址 0x00000000 对应的 页表项。也就是说，我们想 临时把虚拟地址 0 映射到 paddr 所指向的物理页
    page_entry_t *entry = get_pte(0, false);    // 获取虚拟地址 0x0 对应的页表
    entry_init(entry, IDX(paddr));              // 初始化该页表项，指向新分配的物理页
    memcpy((void *)0, (void *)page, PAGE_SIZE); // 将原页内容复制到新页中
    entry->present = false;                     // 取消映射
    flush_tlb(0);                           // 刷新 TLB，确保映射关系更新
    return paddr;
}

// 复制当前任务的页目录
page_entry_t *copy_pde() {
    task_t *task = running_task();

    page_entry_t *pde = (page_entry_t *)alloc_kpage(1); // 分配一页作为新的页目录
    memcpy(pde, (void *)task->pde, PAGE_SIZE);

    page_entry_t *entry = &pde[1023];                   // 将最后一个页表指向页目录自己，方便修改
    entry_init(entry, IDX(pde));                        // 初始化该页目录项

    page_entry_t *dentry;
    for(size_t didx = 2; didx < 1023; didx++) {         // 复制内核空间的页表项
        dentry = &pde[didx];                            // 遍历页目录的所有页目录项
        if(!dentry->present) continue;                  // 如果该页目录项不存在，跳过

        page_entry_t *table = (page_entry_t *)(PDE_MASK | (didx << 12));    // 找到可以修改的页表
        for(size_t tidx = 0; tidx < 1024; tidx++) {     // 遍历该页表的所有页表项
            entry = &table[tidx];
            if(!entry->present) continue;            // 如果该页表项不存在，跳过

            // MMIO/设备寄存器等映射的物理地址可能远超实际 RAM，
            // 不在 memory_map 管理范围内（例如 x86 Local APIC: 0xFEE00000）。
            // 这些映射不参与 COW/引用计数，保持原样共享即可。
            if (entry->index >= total_pages) continue;

            assert(memory_map[entry->index] >= 1);   // 验证该物理页已被占用
            entry->write = false;                    // 先将该页表项设置为只读，防止写时错误
            memory_map[entry->index]++;              // 增加该物理页的引用计数
            assert(memory_map[entry->index] < 255);  // 引用计数不能溢出
        }
        u32 paddr = copy_page(table);                  // 复制该页表对应的物理页
        dentry->index = IDX(paddr);                    // 更新页目录项，指向新的页表物理地址
    }
    set_cr3(task->pde);  // 切换回原任务的页目录
    return pde;
}

// 释放当前任务的页目录
void free_pde(){
    task_t *task = running_task();          
    assert(task->uid != KERNEL_USER);       // 只能用户进程释放自己的页目录
    page_entry_t *pde = get_pde();          // 获取当前任务的页目录

    for(size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < USER_STACK_TOP >> 22; didx++){
        page_entry_t *dentry = &pde[didx];
        if(!dentry->present) continue;      // 如果该页目录项不存在，跳过
        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12)); // 找到可以修改的页表(虚拟地址)
        for (size_t tidx = 0; tidx < 1024; tidx++){
            page_entry_t *entry = &pte[tidx];
            if(!entry->present) continue;               // 如果该页表项不存在，跳过
            assert(memory_map[entry->index] >= 1);      // 验证该物理页已被占用
            put_page(PAGE(entry->index));               // 释放该物理页 
        }
        put_page(PAGE(dentry->index));                  // 释放页表对应的物理页
    }
    free_kpage(task->pde, 1);                           // 释放页目录对应的物理页
    LOGK("free pages %d\n", free_pages);    
}

int32 sys_brk(void *addr){
    LOGK("task brk 0x%p\n", addr);
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);                         // 判断brk是否是页开始的位置
    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);         // 判断是否是用户
    assert(KERNEL_MEMORY_SIZE < brk < USER_STACK_BOTTOM);  // 判断brk是否属于用户内存空间
    u32 old_brk = task->brk;   // 获取进程当前的堆内存边界

    // 如果当前边界大于新申请的边界，那就释放内存映射
    if (old_brk > brk) {
        for (u32 addr = brk; addr < old_brk; addr += PAGE_SIZE) {
            unlink_page(addr);
        }
    }
    else if (IDX(brk - old_brk) > free_pages) {     // 如果新的增加brk大于了剩余的空闲页，就返回-1,没有可用内存了。
        return -1;    // out of memory
    }

    task->brk = brk;
    return 0;
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
u32 alloc_kpage(u32 count){
    assert(count > 0); 
    u32 vaddr = scan_page(&kernel_map, count); // 从内核内存位图中扫描 count 个连续的空闲页，返回起始页基地址
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count); 
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count){
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);      // 重置内核内存位图中对应的 count 个页，标记为未占用
    LOGK("FREE  kernel pages 0x%p count %d\n", vaddr, count);
}

// 链接虚拟地址 vaddr 到 一个物理页
void link_page(u32 vaddr){
    ASSERT_PAGE(vaddr);   // 判断虚拟地址为页开始的位置，即最后三位为0
    page_entry_t *pte = get_pte(vaddr, true);   // 获取vaddr对应的页表
    page_entry_t *entry = &pte[TIDX(vaddr)];    // 获取vaddr页框的入口
    task_t *task = running_task();              // 获取当前的进程
    bitmap_t *map = task->vmap;                 // 当前进程的虚拟位图
    u32 index = IDX(vaddr);                     // 获取vaddr对应的位图索引，标记这一页是否被占用

    if (entry->present) {
        assert(bitmap_test(map, index));  // 判断位图中这一位是否为1
        return; // 页面已存在，说明该虚拟地址已经被映射，直接返回
    }
    assert(!bitmap_test(map, index));  
    bitmap_set(map, index, true);       // 更新虚拟内存位图，标记该页为已占用
    u32 paddr = get_page();             // 分配一页物理内存
    entry_init(entry, IDX(paddr));      // 初始化页表项，建立映射关系
    flush_tlb(vaddr);                   // 刷新该虚拟地址对应的 TLB
    LOGK("LINK from 0x%p to 0x%p\n", vaddr, paddr);
}

// 解除虚拟地址 vaddr 对应的物理页映射
void unlink_page(u32 vaddr){
    ASSERT_PAGE(vaddr);         // 判断虚拟地址为页开始的位置，即最后三位为0
    page_entry_t *pte = get_pte(vaddr, true);   // 获取vaddr对应的页表 
    page_entry_t *entry = &pte[TIDX(vaddr)];    // 获取vaddr页框的入口
    task_t *task = running_task();          // 获取当前的进程
    bitmap_t *map = task->vmap;             // 当前进程的虚拟位图
    u32 index = IDX(vaddr);                 // 获取vaddr对应的位图索引，标记这一页是否被占用

    if (!entry->present){
        assert(!bitmap_test(map, index));
        return; // 页面不存在，说明该虚拟地址没有被映射，直接返回
    }
    assert(entry->present && bitmap_test(map, index));  // 页面存在，且位图中该页被标记为已占用
    entry->present = false;             // 取消页表项的存在标志    
    bitmap_set(map, index, false);      // 更新虚拟内存位图，标记该页为未占用
    u32 paddr = PAGE(entry->index);     // 获取该页表项对应的物理页地址
    DEBUGK("UNLINK from 0x%p to 0x%p\n", vaddr, paddr);
   
    put_page(paddr);    // 函数内部做了判断物理内存是否被多次引用
    flush_tlb(vaddr);   // 刷新该虚拟地址对应的 TLB
}

// 将虚拟地址 vaddr 映射到指定的物理地址 paddr，带标志位
void map_page_fixed(u32 vaddr, u32 paddr, u32 flags){
    ASSERT_PAGE(vaddr);
    ASSERT_PAGE(paddr);
    page_entry_t *pte = get_pte(vaddr, true);       // 获取vaddr对应的页表
    page_entry_t *entry = &pte[TIDX(vaddr)];        // 获取vaddr页框的入口
    assert(!entry->present);                        // 页面必须不存在

    entry_init_flags(entry, IDX(paddr), flags|PAGE_PRESENT);    // 初始化页表项，建立映射关系
    flush_tlb(vaddr);                                           // 刷新该虚拟地址对应的 TLB
    LOGK("MAP fixed from 0x%p to 0x%p\n", vaddr, paddr);
}

// 解除虚拟地址 vaddr 对应的物理页映射，固定映射版本(仅取消映射，不释放物理页)
void unmap_page_fixed(u32 vaddr){
    ASSERT_PAGE(vaddr);
    page_entry_t *pte = get_pte(vaddr, false);      // 获取vaddr对应的页表
    page_entry_t *entry = &pte[TIDX(vaddr)];        // 获取vaddr页框的入口
    assert(entry->present);                         // 页面必须存在

    entry->present = false;                     // 取消页表项的存在标志    
    u32 paddr = PAGE(entry->index);             // 获取该页表项对应的物理页地址
    DEBUGK("UNMAP fixed from 0x%p to 0x%p\n", vaddr, paddr);
   
    // put_page(paddr);     // 
    flush_tlb(vaddr);       // 刷新该虚拟地址对应的 TLB
}

#pragma pack(1) 
typedef struct page_error_code_t {
    u8 present : 1; 
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} page_error_code_t;
#pragma pack()

void page_fault_handler(
    u32 vector,     // 中断向量号
    u32 edi, u32 esi, u32 ebp, u32 esp,     // 通用寄存器
    u32 ebx, u32 edx, u32 ecx, u32 eax,     // 通用寄存器
    u32 gs, u32 fs, u32 es, u32 ds,         // 段寄存器
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags // 中断信息
){
    assert(vector == 0x0e);     // 缺页异常中断号 14 (0x0e)
    u32 vaddr = get_cr2();      // 获取导致缺页异常的线性地址
    LOGK("Page fault at address 0x%p, eip 0x%p, error code 0x%p\n", vaddr, eip, error);
    page_error_code_t *code = (page_error_code_t *)&error;  // 解析错误代码
    
    task_t *task = running_task();
    assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr <= USER_STACK_TOP); // 缺页地址必须在内核内存和用户栈顶之间
    
    // 写时复制缺页, task_fork后用户页被设为只读，共享物理页；
    // 对子进程的写访问会触发页存在但不可写，这是对应处理分支的入口，用于复制独立物理页。
    if (code->present) {
        assert(code->write);    // 必须是写访问引起的缺页异常
        page_entry_t *pte = get_pte(vaddr, false);  // 获取vaddr对应的页表
        page_entry_t *entry = &pte[TIDX(vaddr)];    // 获取vaddr页框的入口
        assert(entry->present);                     // 页面必须存在
        assert(memory_map[entry->index] >= 1);      // 物理页必须被占用

        if(memory_map[entry->index] == 1){      // 仅被一个进程引用,直接提升写权限
            entry->write = true;
            LOGK("Write permission granted for address 0x%p\n", vaddr);
        }
        else{   // 被多个进程引用，执行写时复制
            void *page = (void *)PAGE(IDX(vaddr));   // 获取该虚拟地址对应的页开始位置
            u32 paddr = copy_page(page);             // 复制该页内容到新页
            memory_map[entry->index]--;              // 减少原物理页的引用计数
            entry_init(entry, IDX(paddr));           // 更新页表项，指向新的物理页
            flush_tlb(vaddr);                        // 刷新该虚拟地址对应的 TLB
            LOGK("Copy-on-write for address 0x%p\n", vaddr);
        }
        return;
    }

    // 仅当页面不存在且访问地址在用户栈范围内时，才进行页面链接操作
    if(!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BOTTOM)){
        u32 page = PAGE(IDX(vaddr));    // 计算出对应的页对齐地址
        link_page(page);                // 链接该页
        return;
    }
    panic("Page fault can not be handled!!!");
}

