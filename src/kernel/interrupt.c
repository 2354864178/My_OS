#include <onix/debug.h>
#include <onix/global.h>
#include <onix/interrupt.h>
#include <onix/printk.h>
#include <onix/io.h>
#include <onix/mmio.h>
#include <onix/assert.h>
#include <onix/devicetree.h>
#include <onix/apic.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ENTRY_SIZE 0x30 // 中断处理函数数量

#define PIC_M_CTRL 0x20 // 主片的控制端口
#define PIC_M_DATA 0x21 // 主片的数据端口
#define PIC_S_CTRL 0xa0 // 从片的控制端口
#define PIC_S_DATA 0xa1 // 从片的数据端口
#define PIC_EOI 0x20    // 通知中断控制器中断结束

typedef struct pic_dt_info
{
    bool present;   // 信息是否有效
    u32 m_ctrl;     // 主片控制端口
    u32 m_data;     // 主片数据端口
    u32 s_ctrl;     // 从片控制端口
    u32 s_data;     // 从片数据端口
    u32 cells;      // 中断描述符单元数
} pic_dt_info_t;

static pic_dt_info_t pic_dt;

// 解析设备树中的 8259A 信息，仅用于与硬编码端口对比，当前不影响行为
static void pic_dt_probe(void)
{
    void *val; u32 len;
    const char *paths[] = {"/interrupt-controller@20"};

    if (dtb_get_prop_any(paths, 1, "reg", &val, &len) == 0 && len >= 8)
    {
        u32 *cells = (u32 *)val;
        // reg 按 <addr size> 成对出现。主片覆盖 0x20/0x21，从片覆盖 0xa0/0xa1。
        u32 m_base = dt_be32_read(&cells[0]);
        u32 m_size = dt_be32_read(&cells[1]);
        pic_dt.m_ctrl = m_base;
        pic_dt.m_data = m_base + (m_size > 1 ? 1 : 0);

        if (len >= 16)
        {
            u32 s_base = dt_be32_read(&cells[2]);
            u32 s_size = dt_be32_read(&cells[3]);
            pic_dt.s_ctrl = s_base;
            pic_dt.s_data = s_base + (s_size > 1 ? 1 : 0);
        }
        pic_dt.present = true;
        LOGK("DT pic: m_ctrl 0x%x (code 0x%x), s_ctrl 0x%x (code 0x%x) \n",
             pic_dt.m_ctrl, PIC_M_CTRL, pic_dt.s_ctrl, PIC_S_CTRL);
    }   LOGK("DT pic: m_data 0x%x (code 0x%x), s_data 0x%x (code 0x%x) \n",
             pic_dt.m_data, PIC_M_DATA, pic_dt.s_data, PIC_S_DATA);

    if (dtb_get_prop_any(paths, 1, "#interrupt-cells", &val, &len) == 0 && len >= 4)
    {
        pic_dt.cells = dt_be32_read(val);
        pic_dt.present = true;
        LOGK("DT pic: #interrupt-cells %u\n\n", pic_dt.cells);
    }
}

gate_t idt[IDT_SIZE];   // 中断描述符表
pointer_t idt_ptr;      // 中断描述符表指针

handler_t handler_table[IDT_SIZE];                  // 中断处理函数表
extern handler_t handler_entry_table[ENTRY_SIZE];   // 中断处理函数入口表
extern void syscall_handler();                      // 系统调用处理函数入口
extern void interrupt_handler();                    // 中断处理函数入口
extern void page_fault_handler();                   // 缺页异常处理函数入口

static char *messages[] = {     // 异常信息字符串数组
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};

// 写 Local APIC 寄存器
static _inline void lapic_write32(uintptr_t reg, u32 value){
    mmio_write32((uintptr_t)(LAPIC_BASE_PHYS + reg), value);
}

// 读 Local APIC 寄存器
static _inline u32 lapic_read32(uintptr_t reg){
    return mmio_read32((uintptr_t)(LAPIC_BASE_PHYS + reg));
}

// 读 I/O APIC 寄存器
static _inline u32 ioapic_read32(u32 index){
    uintptr_t regsel = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_REGSEL);   // 选址寄存器
    uintptr_t window = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_WINDOW);   // 窗口寄存器
    mmio_write32(regsel, index);    // 选中要读取的寄存器
    return mmio_read32(window);     // 读取寄存器的值
}

// 写 I/O APIC 寄存器
static _inline void ioapic_write32(u32 index, u32 value){
    uintptr_t regsel = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_REGSEL);
    uintptr_t window = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_WINDOW);
    mmio_write32(regsel, index);
    mmio_write32(window, value);
}

// 写 Local APIC 的 EOI 寄存器完成中断结束确认。
static _inline void lapic_eoi(void){
    lapic_write32(LAPIC_REG_EOI, 0);
}

// 写 I/O APIC 重定向表项
static void ioapic_write_redir(u32 irq, u64 entry){
    u32 low_index = IOAPIC_REDTBL_BASE + irq * 2;   // 重定向表项低 32 位索引
    u32 high_index = low_index + 1;                 // 重定向表项高 32 位索引

    // 先写高 32 位，再写低 32 位：避免在低 32 位写入瞬间出现短暂的无效目的地。
    ioapic_write32(high_index, (u32)((entry >> 32) & 0xFFFFFFFFu)); // 写高 32 位
    ioapic_write32(low_index, (u32)(entry & 0xFFFFFFFFu));          // 写低 32 位
}

// 传统 ISA IRQ 到 IOAPIC 输入引脚（GSI）的最小映射。
// 在常见 PC/QEMU 配置中，PIT(IRQ0) 会被 override 到 GSI2。
static _inline u32 ioapic_pin_from_isa_irq(u32 irq){
    if (irq == IRQ_CLOCK) return 2;     // IRQ0 -> pin 2
    if (irq == IRQ_CASCADE) return 0;   // IRQ2 -> pin 0（常见为 ExtINT/cascade）
    return irq;
}

// 初始化 I/O APIC 的 IRQ0~IRQ15 重定向表（最小版：全部 masked，edge/high，fixed/physical）。
// 之后由各个驱动通过 set_interrupt_mask(irq, true) 来逐个放行。
static void ioapic_init_irq0_15(void) {
    u32 apic_id = (lapic_read32(LAPIC_REG_ID) >> 24) & 0xFFu;   // 本地 APIC ID

    for (u32 irq = 0; irq < 16; irq++) {
        u32 pin = ioapic_pin_from_isa_irq(irq);
        u64 entry = (u64)(APIC_IRQ_TO_VECTOR(irq) & 0xFFu) |    // 设置向量号
                    IOAPIC_REDIR_DELIV_FIXED |                  // 固定模式
                    IOAPIC_REDIR_DEST_PHYSICAL |                // 物理模式
                    IOAPIC_REDIR_POLARITY_HIGH |                // 高电平有效
                    IOAPIC_REDIR_TRIGGER_EDGE |                 // 边沿触发
                    IOAPIC_REDIR_MASKED |                       // 屏蔽
                    IOAPIC_REDIR_DEST(apic_id);                 // 目的地 APIC ID
        ioapic_write_redir(pin, entry);                         // 写入重定向表项
    }
}

// 注册中断处理函数
void set_interrupt_handler(u32 irq, handler_t handler){
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;   // 注册中断处理函数
}

// 设置中断屏蔽位
void set_interrupt_mask(u32 irq, bool enable){
    assert(irq < 16);
    // I/O APIC 的寄存器访问方式：先写 IOREGSEL 选中内部寄存器索引，再从 IOWIN 读写数据。
    uintptr_t regsel = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_REGSEL);
    uintptr_t window = (uintptr_t)(IOAPIC_BASE_PHYS + IOAPIC_WINDOW);

    u32 pin = ioapic_pin_from_isa_irq(irq);
    u32 redir_low_index = IOAPIC_REDTBL_BASE + pin * 2;

    // 读 redirection entry 的低 32 位
    mmio_write32(regsel, redir_low_index);                  // 选中重定向表项低 32 位
    u32 low = mmio_read32(window);                          // 读取当前值

    if (enable) low &= ~(1u << IOAPIC_REDIR_MASK_SHIFT);    // 取消屏蔽
    else low |= (1u << IOAPIC_REDIR_MASK_SHIFT);            // 屏蔽

    // 写回低 32 位（不改变 vector/trigger/polarity 等其他位）
    mmio_write32(regsel, redir_low_index);                  // 选中重定向表项低 32 位
    mmio_write32(window, low);                              // 写回修改后的值
}


// 初始化中断控制器
void lapic_init(){
    lapic_write32(LAPIC_REG_SVR, LAPIC_SVR_ENABLE | APIC_SPURIOUS_VECTOR);
    lapic_write32(LAPIC_REG_TPR, 0);
    lapic_eoi();
}

// 异常处理函数
void exception_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    char *message = NULL;
    if (vector < 22) message = messages[vector];    // 获取异常信息字符串

    else message = messages[15];    // 15 是缺页异常，其他异常未定义

    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);

    bool hanging = true;

    // 阻塞
    while (hanging)
        ;
    // 通过 EIP 的值应该可以找到出错的位置
    // 也可以在出错时，可以将 hanging 在调试器中手动设置为 0
    // 然后在下面 return 打断点，单步调试，找到出错的位置
    return;
}

// 清除 IF 位，返回设置之前的值
bool interrupt_disable()
{
    asm volatile(
        "pushfl\n"        // 将当前 eflags 压入栈中
        "cli\n"           // 清除 IF 位，此时外中断已被屏蔽
        "popl %eax\n"     // 将刚才压入的 eflags 弹出到 eax
        "shrl $9, %eax\n" // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n" // 只需要 IF 位
    );
}

// 获得 IF 位
bool get_interrupt_state()
{
    asm volatile(
        "pushfl\n"        // 将当前 eflags 压入栈中
        "popl %eax\n"     // 将压入的 eflags 弹出到 eax
        "shrl $9, %eax\n" // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n" // 只需要 IF 位
    );
}

// 设置 IF 位
void set_interrupt_state(bool state)
{
    if (state)
        asm volatile("sti\n");  // 设置 IF 位，允许外中断
    else
        asm volatile("cli\n");  // 清除 IF 位，屏蔽外中断
}

void default_handler(int vector){
    send_eoi(vector);
    DEBUGK("[%x] default interrupt called ...\n", vector);
}

// 向中断控制器发送 EOI。
// APIC 路线：对外部 IRQ 向量发送 Local APIC EOI。
void send_eoi(int vector){
    // 仅对 IRQ0~IRQ15 对应的向量发送 EOI（默认 IRQ_BASE=0x20）。
    if ((u32)vector >= IRQ_MASTER_NR && (u32)vector < (IRQ_MASTER_NR + 16))
        lapic_eoi();
}

// 初始化中断描述符表 IDT
void idt_init(){
    for (size_t i = 0; i < IDT_SIZE; i++)
    {
        gate_t *gate = &idt[i]; // 获取 IDT 中的第 i 个门描述符
        handler_t handler = handler_entry_table[i]; // 获取对应的中断处理函数地址

        gate->offset0 = (u32)handler & 0xffff;          // 段内偏移 0 ~ 15 位
        gate->offset1 = ((u32)handler >> 16) & 0xffff;  // 段内偏移 16 ~ 31 位
        gate->selector = 1 << 3; // 代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态
        gate->present = 1;       // 有效
    }

    for (size_t i = 0; i < 0x20; i++) {
        handler_table[i] = exception_handler;
    }

    handler_table[0x0e] = page_fault_handler; // 缺页异常单独处理

    for (size_t i = 0x20; i < ENTRY_SIZE; i++) {
        handler_table[i] = default_handler;
    }

    gate_t *syscall_gate = &idt[0x80];
    syscall_gate->offset0 = (u32)syscall_handler & 0xffff;
    syscall_gate->offset1 = ((u32)syscall_handler >> 16) & 0xffff;
    syscall_gate->selector = 1 << 3; // 代码段
    syscall_gate->reserved = 0;      // 保留不用
    syscall_gate->type = 0b1110;     // 中断门（进入内核时清 IF；本内核调度/阻塞路径要求关中断）
    syscall_gate->segment = 0;       // 系统段
    syscall_gate->DPL = 3;           // 用户态
    syscall_gate->present = 1;       // 有效

    idt_ptr.base = (u32)idt;            // IDT 表地址
    idt_ptr.limit = sizeof(idt) - 1; 
    // BMB;
    asm volatile("lidt idt_ptr\n");
}

void interrupt_init()
{
    assert(dtb_node_enabled("/interrupt-controller@20"));
    pic_dt_probe();
    idt_init();

    // APIC 路线：先开 LAPIC，再配置 IOAPIC 的 IRQ0/IRQ1 路由。
    lapic_init();
    ioapic_init_irq0_15();

    // 将外部 IRQ 从 8259A 路由到 APIC（IMCR）。
    // outb(0x22, 0x70);
    // outb(0x23, 0x01);

    // 屏蔽 legacy PIC，避免 APIC 与 PIC 同时产生外部中断（双重触发）。
    outb(PIC_M_DATA, 0xFF);
    outb(PIC_S_DATA, 0xFF);
}