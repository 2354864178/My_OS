[bits 32]

extern kernel_init

global _start
_start:
    ; mov byte [0xb81E0],'3'
    ; mov byte [0xb81E2],'2'
    ; mov byte [0xb81E4],'m'
    ; mov byte [0xb81E6],'o'
    ; mov byte [0xb81E8],'d'
    call kernel_init
    ; xchg bx, bx
    ; int 0x80
    jmp $