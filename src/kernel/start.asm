[bits 32]

extern console_init
extern memory_init
extern kernel_init
extern gdt_init

global _start
_start:
    ; mov byte [0xb81E0],'3'
    ; mov byte [0xb81E2],'2'
    ; mov byte [0xb81E4],'m'
    ; mov byte [0xb81E6],'o'
    ; mov byte [0xb81E8],'d'
    push ebx;
    push eax;

    call console_init
    call gdt_init
    call memory_init
    call kernel_init
    ; xchg bx, bx
    ; int 0x80
    jmp $