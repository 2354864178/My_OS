#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/console.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/global.h>
#include <onix/interrupt.h>
#include <onix/stdlib.h>

void kernel_init(){
    // BMB;
    console_init();
    gdt_init();
    
    interrupt_init();
    task_init();
    
    return;
}