#include <onix/interrupt.h>
#include <onix/io.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/mutex.h>
#include <onix/fifo.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/devicetree.h>
#include <onix/string.h>
#include <onix/types.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define KEYBOARD_DATA_PORT 0x60 // 键盘数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 键盘控制端口

#define KEYBOARD_CMD_LED 0xED // 设置 LED 状态
#define KEYBOARD_CMD_ACK 0xFA // ACK

#define INV 0 // 不可见字符

#define CODE_PRINT_SCREEN_DOWN 0xB7 // Print Screen 按键按下扫描码

static reentrant_mutex_t lock;    // 锁
static task_t *waiter; // 等待输入的任务

#define BUFFER_SIZE 64        // 输入缓冲区大小
static char buf[BUFFER_SIZE]; // 输入缓冲区
static fifo_t fifo;           // 循环队列

typedef struct keyboard_dt_info
{
    bool present;       // 信息是否有效
    u32 data_port;      // 数据端口
    u32 ctrl_port;      // 控制端口
    u32 irq;            // 中断号
    char keymap[16];    // 键盘映射（当前未用）
} keyboard_dt_info_t;

static keyboard_dt_info_t kbd_dt;

// 键盘扫描码枚举类型
typedef enum{
    KEY_NONE,       // 0 表示无效按键
    KEY_ESC,        // 1 表示 Esc 键
    KEY_1,          // 2~10 表示数字键盘上的数字
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,   
    KEY_MINUS,      // 11 表示减号键
    KEY_EQUAL,      // 12 表示等号键
    KEY_BACKSPACE,  // 13 表示退格键
    KEY_TAB,        // 14 表示 Tab 键
    KEY_Q,          // 15~24 表示字母键
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,          
    KEY_BRACKET_L,  // 25 表示左中括号键
    KEY_BRACKET_R,  // 26 表示右中括号键
    KEY_ENTER,      // 27 表示回车键
    KEY_CTRL_L,     // 28 表示左 Ctrl 键
    KEY_A,          // 30~38 表示字母键
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,     
    KEY_SEMICOLON,  // 39 表示分号键
    KEY_QUOTE,      // 40 表示单引号键
    KEY_BACKQUOTE,  // 41 表示反引号键
    KEY_SHIFT_L,    // 42 表示左 Shift 键
    KEY_BACKSLASH,  // 43 表示反斜杠键
    KEY_Z,          // 44~50 表示字母键
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_COMMA,      // 51 表示逗号键
    KEY_POINT,      // 52 表示句点键
    KEY_SLASH,      // 53 表示斜杠键
    KEY_SHIFT_R,    // 54 表示右 Shift 键
    KEY_STAR,       // 55 表示小键盘上的星号键
    KEY_ALT_L,      // 56 表示左 Alt 键
    KEY_SPACE,      // 57 表示空格键
    KEY_CAPSLOCK,   // 58 表示大写锁定键
    KEY_F1,         // 59~68 表示功能键 F1~F10
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_NUMLOCK,    // 69 表示数字锁定键
    KEY_SCRLOCK,    // 70 表示滚动锁定键
    KEY_PAD_7,      // 71~83 表示小键盘上的数字和运算符
    KEY_PAD_8,
    KEY_PAD_9,
    KEY_PAD_MINUS,  // 74 表示小键盘上的减号键
    KEY_PAD_4,
    KEY_PAD_5,
    KEY_PAD_6,
    KEY_PAD_PLUS,   // 78 表示小键盘上的加号键
    KEY_PAD_1,
    KEY_PAD_2,
    KEY_PAD_3,
    KEY_PAD_0,
    KEY_PAD_POINT,  // 83 表示小键盘上的句点键
    KEY_54,         // 84~89 表示其他控制键
    KEY_55,
    KEY_56,
    KEY_F11,        
    KEY_F12,
    KEY_59,         
    KEY_WIN_L,
    KEY_WIN_R,
    KEY_CLIPBOARD,  
    KEY_5D,         
    KEY_5E,         
    KEY_PRINT_SCREEN   // 自定义按键，为和 keymap 索引匹配
}KEY;

// keymap 每行 4 列：无 Shift 字符、带 Shift 字符、扩展键标志、保留标志（当前未用）
static char keymap[][4] = {
    /* 扫描码 未与 shift 组合  与 shift 组合 以及相关状态 */
    /* ---------------------------------- */
    /* 0x00 */ {INV, INV, false, false},    // 空
    /* 0x01 */ {0x1b, 0x1b, false, false},  // ESC 键
    /* 0x02 */ {'1', '!', false, false},    // 数字 1
    /* 0x03 */ {'2', '@', false, false},
    /* 0x04 */ {'3', '#', false, false},
    /* 0x05 */ {'4', '$', false, false},
    /* 0x06 */ {'5', '%', false, false},
    /* 0x07 */ {'6', '^', false, false},
    /* 0x08 */ {'7', '&', false, false},
    /* 0x09 */ {'8', '*', false, false},
    /* 0x0A */ {'9', '(', false, false},
    /* 0x0B */ {'0', ')', false, false},
    /* 0x0C */ {'-', '_', false, false},
    /* 0x0D */ {'=', '+', false, false},
    /* 0x0E */ {'\b', '\b', false, false}, // 退格
    /* 0x0F */ {'\t', '\t', false, false}, // 制表
    /* 0x10 */ {'q', 'Q', false, false},
    /* 0x11 */ {'w', 'W', false, false},
    /* 0x12 */ {'e', 'E', false, false},
    /* 0x13 */ {'r', 'R', false, false},
    /* 0x14 */ {'t', 'T', false, false},
    /* 0x15 */ {'y', 'Y', false, false},
    /* 0x16 */ {'u', 'U', false, false},
    /* 0x17 */ {'i', 'I', false, false},
    /* 0x18 */ {'o', 'O', false, false},
    /* 0x19 */ {'p', 'P', false, false},
    /* 0x1A */ {'[', '{', false, false},
    /* 0x1B */ {']', '}', false, false},
    /* 0x1C */ {'\n', '\n', false, false}, // 回车
    /* 0x1D */ {INV, INV, false, false},   // 左 Ctrl
    /* 0x1E */ {'a', 'A', false, false},
    /* 0x1F */ {'s', 'S', false, false},
    /* 0x20 */ {'d', 'D', false, false},
    /* 0x21 */ {'f', 'F', false, false},
    /* 0x22 */ {'g', 'G', false, false},
    /* 0x23 */ {'h', 'H', false, false},
    /* 0x24 */ {'j', 'J', false, false},
    /* 0x25 */ {'k', 'K', false, false},
    /* 0x26 */ {'l', 'L', false, false},
    /* 0x27 */ {';', ':', false, false},
    /* 0x28 */ {'\'', '"', false, false},
    /* 0x29 */ {'`', '~', false, false},
    /* 0x2A */ {INV, INV, false, false}, // 左 Shift
    /* 0x2B */ {'\\', '|', false, false},
    /* 0x2C */ {'z', 'Z', false, false},
    /* 0x2D */ {'x', 'X', false, false},
    /* 0x2E */ {'c', 'C', false, false},
    /* 0x2F */ {'v', 'V', false, false},
    /* 0x30 */ {'b', 'B', false, false},
    /* 0x31 */ {'n', 'N', false, false},
    /* 0x32 */ {'m', 'M', false, false},
    /* 0x33 */ {',', '<', false, false},
    /* 0x34 */ {'.', '>', false, false},
    /* 0x35 */ {'/', '?', false, false},
    /* 0x36 */ {INV, INV, false, false},  // 右 Shift
    /* 0x37 */ {'*', '*', false, false},  // 小键盘 *
    /* 0x38 */ {INV, INV, false, false},  // 左 Alt
    /* 0x39 */ {' ', ' ', false, false},  // 空格
    /* 0x3A */ {INV, INV, false, false},  // 大写锁定
    /* 0x3B */ {INV, INV, false, false},  // F1
    /* 0x3C */ {INV, INV, false, false},  // F2
    /* 0x3D */ {INV, INV, false, false},  // F3
    /* 0x3E */ {INV, INV, false, false},  // F4
    /* 0x3F */ {INV, INV, false, false},  // F5
    /* 0x40 */ {INV, INV, false, false},  // F6
    /* 0x41 */ {INV, INV, false, false},  // F7
    /* 0x42 */ {INV, INV, false, false},  // F8
    /* 0x43 */ {INV, INV, false, false},  // F9
    /* 0x44 */ {INV, INV, false, false},  // F10
    /* 0x45 */ {INV, INV, false, false},  // 数字锁定
    /* 0x46 */ {INV, INV, false, false},  // 滚动锁定
    /* 0x47 */ {'7', INV, false, false},  // 小键盘 7 / Home
    /* 0x48 */ {'8', INV, false, false},  // 小键盘 8 / 上
    /* 0x49 */ {'9', INV, false, false},  // 小键盘 9 / PageUp
    /* 0x4A */ {'-', '-', false, false},  // 小键盘 -
    /* 0x4B */ {'4', INV, false, false},  // 小键盘 4 / 左
    /* 0x4C */ {'5', INV, false, false},  // 小键盘 5
    /* 0x4D */ {'6', INV, false, false},  // 小键盘 6 / 右
    /* 0x4E */ {'+', '+', false, false},  // 小键盘 +
    /* 0x4F */ {'1', INV, false, false},  // 小键盘 1 / End
    /* 0x50 */ {'2', INV, false, false},  // 小键盘 2 / 下
    /* 0x51 */ {'3', INV, false, false},  // 小键盘 3 / PageDown
    /* 0x52 */ {'0', INV, false, false},  // 小键盘 0 / Insert
    /* 0x53 */ {'.', 0x7F, false, false}, // 小键盘 . / Delete
    /* 0x54 */ {INV, INV, false, false},  // 预留
    /* 0x55 */ {INV, INV, false, false},  // 预留
    /* 0x56 */ {INV, INV, false, false},  // 预留
    /* 0x57 */ {INV, INV, false, false},  // F11
    /* 0x58 */ {INV, INV, false, false},  // F12
    /* 0x59 */ {INV, INV, false, false},  // 预留
    /* 0x5A */ {INV, INV, false, false},  // 预留
    /* 0x5B */ {INV, INV, false, false},  // 左 Win
    /* 0x5C */ {INV, INV, false, false},  // 右 Win
    /* 0x5D */ {INV, INV, false, false},  // 应用/菜单
    /* 0x5E */ {INV, INV, false, false},  // 预留

    // Print Screen 强制映射（原码 0xB7）
    /* 0x5F */ {INV, INV, false, false}, // 打印屏幕
};

static bool capslock_state; // 大写锁定
static bool scrlock_state;  // 滚动锁定
static bool numlock_state;  // 数字锁定
static bool extcode_state;  // 扩展码状态
#define ctrl_state (keymap[KEY_CTRL_L][2] || keymap[KEY_CTRL_L][3])     // CTRL 键状态
#define alt_state (keymap[KEY_ALT_L][2] || keymap[KEY_ALT_L][3])        // ALT 键状态
#define shift_state (keymap[KEY_SHIFT_L][2] || keymap[KEY_SHIFT_R][2])  // SHIFT 键状态

static void keyboard_dt_probe(void)
{
    void *val; u32 len;
    const char *paths[] = {"/keyboard@60"};

    if (dtb_get_prop_any(paths, 1, "reg", &val, &len) == 0 && len >= 8)
    {
        u32 *cells = (u32 *)val;
        kbd_dt.data_port = dt_be32_read(&cells[0]);
        if (len >= 16)
            kbd_dt.ctrl_port = dt_be32_read(&cells[2]);
        kbd_dt.present = true;
        LOGK("DT keyboard: data 0x%x (code 0x%x), ctrl 0x%x (code 0x%x)\n",
             kbd_dt.data_port, KEYBOARD_DATA_PORT, kbd_dt.ctrl_port, KEYBOARD_CTRL_PORT);
    }

    if (dtb_get_prop_any(paths, 1, "interrupts", &val, &len) == 0 && len >= 4)
    {
        kbd_dt.irq = dt_be32_read(val);
        kbd_dt.present = true;
        LOGK("DT keyboard: irq %u (code %u)\n", kbd_dt.irq, IRQ_KEYBOARD);
    }

    if (dtb_get_prop_any(paths, 1, "keymap", &val, &len) == 0 && len > 0)
    {
        size_t n = len;
        if (n >= sizeof(kbd_dt.keymap)) n = sizeof(kbd_dt.keymap) - 1;
        memcpy(kbd_dt.keymap, val, n);
        kbd_dt.keymap[n] = '\0';
        kbd_dt.present = true;
        LOGK("DT keyboard: keymap %s\n\n", kbd_dt.keymap);
    }
}

// 等待键盘控制器准备就绪
static void keyboard_wait()
{
    u8 state;
    do{
        state = inb(kbd_dt.ctrl_port);  // 读取键盘控制端口
    } while (state & 0x02);             // 读取键盘缓冲区，直到为空
}

// 等待键盘控制器发送 ACK 确认
static void keyboard_ack()
{
    u8 state;
    do{
        state = inb(kbd_dt.data_port);    // 读取键盘数据端口
    } while (state != KEYBOARD_CMD_ACK);    // 直到收到 ACK 确认
}

static void set_leds()
{
    u8 leds = (capslock_state << 2) | (numlock_state << 1) | scrlock_state;
    keyboard_wait();
    
    outb(kbd_dt.data_port, KEYBOARD_CMD_LED); // 设置 LED 命令
    keyboard_ack();

    keyboard_wait();
    
    outb(kbd_dt.data_port, leds);             // 设置 LED 灯状态
    keyboard_ack();
}


void keyboard_handler(int vector){
    assert(vector == 0x21);
    send_eoi(vector);                       // 发送中断处理完成信号
    u16 scancode = inb(kbd_dt.data_port); // 读取扫描码；>0x80 表示按键释放
    u8 ext = 2; // 扩展码偏移量

    // 处理扩展码
    if(scancode == 0xE0){
        extcode_state = true;   
        return;
    }

    if(extcode_state){
        scancode |= 0xE000;     // 标记为扩展码
        extcode_state = false;  // 重置扩展码状态
        ext = 3;                // 扩展码偏移量
    }

    // 处理按键释放事件
    u16 makecode = (scancode & 0x7f);       // 获取按键按下时的扫描码
    if(makecode == CODE_PRINT_SCREEN_DOWN) makecode = KEY_PRINT_SCREEN; // 处理 Print Screen 键
    if(makecode > KEY_PRINT_SCREEN) return; // 非法扫描码，直接返回
    if(scancode & 0x80){ // 按键释放事件
        keymap[makecode][ext] = false; // 更新按键状态
        return;
    }

    // 处理按键按下事件
    keymap[makecode][ext] = true; // 更新按键状态

    // 是否需要设置 LED 灯
    bool led = false;
    if (makecode == KEY_NUMLOCK){
        numlock_state = !numlock_state;
        led = true;
    }
    else if (makecode == KEY_CAPSLOCK){
        capslock_state = !capslock_state;
        led = true;
    }
    else if (makecode == KEY_SCRLOCK){
        scrlock_state = !scrlock_state;
        led = true;
    }

    if(led) set_leds();

    // 处理普通按键输入
    bool shift = false;
    if(capslock_state && ('a' <= keymap[makecode][0] && keymap[makecode][0] <= 'z')){
        shift = !shift;             // 大写锁定影响字母键
    }
    if(shift_state) shift = !shift; // Shift 键影响所有按键

    char ch;
    if(ext == 3 && makecode == KEY_SPACE) ch = keymap[makecode][1]; // 处理扩展空格键

    else ch = keymap[makecode][shift];  // 根据 shift 状态选择字符
    if(ch == INV) return;               // 不可见字符，直接返回

    // LOGK("keyboard input 0x%c\n", ch);

    fifo_put(&fifo, ch);
    if (waiter != NULL){
        task_unlock(waiter);
        waiter = NULL;
    }
}

u32 keyboard_read(void *dev, char *buf, u32 count)
{
    reentrant_mutex_lock(&lock);    // 加锁
    int nr = 0;
    while (nr < count)
    {
        while (fifo_empty(&fifo)){
            waiter = running_task();  // 如果队列没有数据，就阻塞进行等待。
            task_block(waiter, NULL, TASK_BLOCKED); // 阻塞当前任务，等待键盘输入
        }
        buf[nr++] = fifo_get(&fifo);    // 从缓冲区获取一个字符
    }
    reentrant_mutex_unlock(&lock);  // 解锁
    return count;
}

void keyboard_init(){
    capslock_state = false;
    scrlock_state = false;
    numlock_state = false;
    extcode_state = false;

    assert(dtb_node_enabled("/keyboard@60"));
    keyboard_dt_probe();

    fifo_init(&fifo, buf, BUFFER_SIZE);
    reentrant_mutex_init(&lock);
    waiter = NULL;

    set_leds();

    set_interrupt_handler(kbd_dt.irq, keyboard_handler);  // 设置键盘中断处理函数
    set_interrupt_mask(kbd_dt.irq, true);                 // 允许键盘中断

    device_install(DEV_CHAR, DEV_KEYBOARD,
        NULL, "keyboard", 0,
        NULL, keyboard_read, NULL);
}




