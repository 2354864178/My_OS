#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/mutex.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define KEYBOARD_DATA_PORT 0x60 // 键盘数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 键盘控制端口

#define INV 0 // 不可见字符

void keyboard_handler(int vector){
    assert(vector == 0x21);
    send_eoi(vector);                       // 发送中断处理完成信号
    u16 scancode = inb(KEYBOARD_DATA_PORT); // 读取扫描码；>0x80 表示按键释放
    LOGK("keyboard input 0x%d\n", scancode);
}

void keyboard_init(){
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler); // 注册键盘中断服务例程
    set_interrupt_mask(IRQ_KEYBOARD, true);                // 使能键盘中断
}
