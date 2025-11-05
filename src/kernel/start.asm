[bits 32]

global _start
_start:
    mov byte [0xb8040], 'K'
    ; call kernel_init
    jmp $