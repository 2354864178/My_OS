extern void clock_init();
extern void console_init();
extern void gdt_init();
extern void interrupt_init();

void kernel_init(){
    // BMB;
    console_init();
    gdt_init();
    interrupt_init();
    // task_init();
    clock_init();
    asm volatile("sti");
    
    return;
}