#include <onix/onix.h>
#include <onix/types.h>
#include <onix/io.h>
#include <onix/console.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>

void kernel_init(){
    console_init();
    assert(3<5);
    assert(3>5);
    // BMB;
    // DEBUGK("debug onix");
    return;
}