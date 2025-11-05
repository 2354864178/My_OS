[bits 32]

extern kernel_init

global _start
_start:
    mov byte [0xb8040], 'K'
    ; call kernel_init
    jmp $