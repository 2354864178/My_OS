[bits 32]

magic   equ 0xe85250d6      ;多重引导2魔数
i386    equ 0               ;32位保护模式标志
length  equ header_end - header_start   ;头部长度

section .multiboot2         ; 多重引导2头部
align 8     ; 8字节对齐
header_start:
    dd magic  ; 魔数
    dd i386   ; 32位保护模式
    dd length ; 头部长度
    dd -(magic + i386 + length); 校验和

    ; 结束标记
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

extern device_init
extern console_init
extern memory_init
extern kernel_init
extern gdt_init
extern gdt_ptr

code_selecter equ (1 << 3)	; 代码段选择子
data_selecter equ (2 << 3)	; 数据段选择子

section .text
global _start
_start:

    push ebx;
    push eax;

    call device_init    ; 设备初始化
    call console_init   ; 控制台初始化
    xchg bx, bx
    call gdt_init       ; 全局描述符表初始化
    xchg bx, bx
    lgdt [gdt_ptr]      ; 加载GDT寄存器
    xchg bx, bx
    jmp code_selecter:_next ; 重新加载代码段寄存器

_next:
    mov ax, data_selecter
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    call memory_init    ; 内存管理初始化
    xchg bx, bx
    mov esp, 0x10000    ; 设置栈顶地址
    xchg bx, bx
    call kernel_init    ; 内核初始化

    ; xchg bx, bx
    ; mov eax, 0 
    ; int 0x80
    jmp $