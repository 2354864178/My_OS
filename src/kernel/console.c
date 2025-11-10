#include <onix/types.h>
#include <onix/io.h>
#include <onix/string.h>

#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器

#define CRT_START_ADDR_H 0xC // 显示内存起始位置 - 高位
#define CRT_START_ADDR_L 0xD // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE     // 光标位置 - 高位
#define CRT_CURSOR_L 0xF     // 光标位置 - 低位

#define MEM_BASE 0xB8000              // 显卡内存起始位置
#define MEM_SIZE 0x4000               // 显卡内存大小
#define MEM_END (MEM_BASE + MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

//  一些符号
#define NUL 0x00
#define ENQ 0x05
#define ESC 0x1B // ESC
#define BEL 0x07 // \a
#define BS 0x08  // \b
#define HT 0x09  // \t
#define LF 0x0A  // \n
#define VT 0x0B  // \v
#define FF 0x0C  // \f
#define CR 0x0D  // \r
#define DEL 0x7F

static u32 screen;  //  当前显示器开始的内存位置
static u32 pos;     //  当前光标的内存位置
static u32 x, y;    //  当前光标的坐标（80x25）
static u8 attr = 7; //  字符的样式
static u16 erase = 0x0720;  //  空格（删除后插入空格）

//  获取 VGA 文本模式下当前屏幕显示缓冲区起始地址
static void get_screen(){
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);   // 告诉VGA：接下来要操作起始地址高位寄存器
    screen = inb(CRT_DATA_REG) << 8;        // 从数据寄存器读高8位，左移8位暂存（占screen的高8位）
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);   // 告诉VGA：接下来要操作起始地址低位寄存器
    screen |= inb(CRT_DATA_REG);            // 读低8位，与高8位合并（screen变为16位完整偏移量）
    
    screen <<= 1;                           // 偏移量×2（因为每个字符占2字节）
    screen += MEM_BASE;                     // 加上显示缓冲区的基地址（0xB8000），得到实际地址
}

//  设置 VGA 文本模式下当前屏幕显示缓冲区起始地址
static void set_screen(){
    outb(CRT_ADDR_REG, CRT_START_ADDR_H);   // 告诉VGA：接下来要操作起始地址高位寄存器
    outb(CRT_DATA_REG, (((screen - MEM_BASE) >> 1)>>8 )& 0xff);        // screen的高8位存入数据寄存器
    outb(CRT_ADDR_REG, CRT_START_ADDR_L);   // 告诉VGA：接下来要操作起始地址低位寄存器
    outb(CRT_DATA_REG, ((screen-MEM_BASE) >> 1) & 0xff);               // screen的低8位存入数据寄存器
}

//  获取光标在屏幕上的具体位置（x 列坐标和 y 行坐标）
static void get_cursor(){
    outb(CRT_ADDR_REG, CRT_CURSOR_H);       // 告诉VGA：接下来要操作光标位置高位寄存器
    pos = inb(CRT_DATA_REG) << 8;           // 从数据寄存器读高8位，左移8位暂存（占screen的高8位）
    outb(CRT_ADDR_REG, CRT_CURSOR_L);       // 告诉VGA：接下来要操作光标位置低位寄存器
    pos |= inb(CRT_DATA_REG);               // 读低8位，与高8位合并（screen变为16位完整偏移量）
    
    get_screen();   //  获取屏幕起始地址

    pos <<= 1;
    pos += MEM_BASE;
    
    u32 delta = (pos - screen) >> 1;    //  计算光标位置相较于屏幕左上角的偏移量
    x = delta % WIDTH;  //  计算x坐标
    y = delta / WIDTH;  //  计算y坐标
}

//  设置光标在屏幕上的具体位置
static void set_cursor(){
    outb(CRT_ADDR_REG, CRT_CURSOR_H);       // 告诉VGA：接下来要操作光标位置高位寄存器
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 9) & 0xff); // pos的高8位存入数据寄存器
    outb(CRT_ADDR_REG, CRT_CURSOR_L);       // 告诉VGA：接下来要操作光标位置低位寄存器
    outb(CRT_DATA_REG, ((pos - MEM_BASE) >> 1) & 0xff); // pos的低8位存入数据寄存器
}

//  清屏
void console_clear(){
    screen = MEM_BASE;  //  屏幕缓存起始到显存起始
    pos = MEM_BASE;     //  光标位置到起始位置
    x = y = 0;          //  坐标归零
    set_cursor();       //  设置屏幕缓存位置
    set_screen();       //  设置光标位置

    u16 *ptr = (u16 *)MEM_BASE;
    while(ptr < (u16 *)MEM_END){
        *ptr++ = erase; //  空格覆盖屏幕
    }
}

// 向上滚屏
static void scroll_up(){
    //  缓冲区有足够空间
    if(screen + SCR_SIZE + ROW_SIZE < MEM_END){
        u32 *ptr = (u32 *)(screen + SCR_SIZE);  //  当前屏幕内容的末尾地址，转为u32*方便一次写2个字符（4字节）
        //  将新增的一行清空（填充空格）
        for(size_t i=0; i<WIDTH; i++){
            *ptr++ = erase;
        }
        screen += ROW_SIZE; //  原来的第二行内容现在变成第一行，所以屏幕起始地址向后移动一行的字节数
        pos += ROW_SIZE;    //  光标需要保持在内容的相对位置，所以也向后移动一行的字节数
    }
    //  缓冲区空间不足，将当前屏幕内容复制到缓冲区开头
    else{
        memcpy((void *)MEM_BASE, (void *)screen, SCR_SIZE);
        pos -= screen - MEM_BASE;   //  光标跟着内容一起搬回缓冲区开头
        screen = MEM_BASE;          //  重置屏幕起始地址为缓冲区开头
    }
    // y--;    //  光标会上滚一行
    set_screen(); 
}

// 光标回车符\r(回到段前)
static void command_cr(){
    pos -= (x << 1);
    x = 0;
    // set_cursor();
}

// 光标换行\n
static void command_lf(){
    // 当前行不是最后一行，直接换行到下一行
    if(y + 1 < HEIGHT){
        y++;               // 光标行坐标+1（移动到下一行）
        pos += ROW_SIZE;   // 光标内存地址+一行的字节数（对应下一行的起始位置）
        set_cursor();
        return;            // 完成换行，退出函数
    }
    // 当前行是最后一行，先滚屏再换行
    scroll_up();
    // y++;
    // set_cursor();
}

// 删除当前光标位置字符
static void command_del(){
    *(u16 *)pos = erase;
}

// 回退光标位置并删除前一个字符
static void command_bs(){
    if(x)
    {
        x--;
        pos -= 2;
        *(u16 *)pos = erase;
        // set_cursor();
    }
}

void console_write(char *buf, u32 count){
    char ch;
    while(count--){
        ch = *buf++;
        switch (ch)
        {
        case NUL:
            break;
        case ENQ:
            break;
        case ESC:
            break;
        case BEL:
            break;
        case BS:
            command_bs();
            break;
        case HT:
            break;
        case LF:
            command_lf();
            command_cr();   
            break;
        case VT:
            break;
        case FF:
            command_lf();
            break;
        case CR:
            command_cr();
            break;
        case DEL:
            command_del();
            break;
        default:
            if(x >= WIDTH){
                x -= WIDTH;
                pos -= ROW_SIZE;
                command_lf();
            }
            *((char *)pos) = ch;
            pos++;
            *((char *)pos) = attr;
            pos++;
            x++;
            // set_cursor();
            break;
            
        }
    }
    //  字符对应的处理函数command_bs等只负责数值计算，光标设置在所有数值计算完成后统一进行
    //  这样每次console_write函数只操作一次硬件，代码简洁性和执行效率的妥协
    set_cursor();
}

void console_init(){
    get_screen();
    get_cursor();
    console_clear();
    
}

