[bits 32]

extern console_init
extern memory_init
extern kernel_init
extern gdt_init

global _start
_start:

    push ebx;
    push eax;

    call console_init   ; 控制台初始化
    call gdt_init       ; 全局描述符表初始化
    call memory_init    ; 内存管理初始化
    ;  pop eax;
    ;  pop ebx;
    call kernel_init    ; 内核初始化

    ; xchg bx, bx
    ; mov eax, 0 
    ; int 0x80
    jmp $