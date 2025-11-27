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
    ; mov esp, 0x100000

    push ebx;
    push eax;

    call console_init
    ; xchg bx, bx
    call gdt_init
    ; xchg bx, bx
    call memory_init
    ; xchg bx, bx
    call kernel_init
    ; xchg bx, bx
    ; int 0x80
    jmp $