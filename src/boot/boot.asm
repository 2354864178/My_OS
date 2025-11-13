[org 0x7c00]
 
; 设置屏幕模式为文本模式，清除屏幕
mov ax, 3
int 0x10

; 初始化段寄存器
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; 断点
; xchg bx, bx

mov si, booting
call print

mov edi, 0x1000; 读取的目标内存
mov ecx, 2; 起始扇区
mov bl, 4; 读取的扇区数

call read_disk

cmp word [0x1000], 0x55aa
jnz error

jmp 0:0x1002

;阻塞
jmp $

read_disk:
	mov dx, 0x1f2
	mov al, bl
	out dx, al

	inc dx; 0x1f3
	mov al, cl
	out dx, al

	inc dx; 0x1f4
	shr ecx, 8
	mov al, cl
	out dx, al

	inc dx; 0x1f5
	shr ecx, 8
	mov al, cl
	out dx, al

	inc dx; x1f6
	shr ecx, 8
	and cl, 0b1111;

	mov al, 0b1110_0000;
	or al, cl
	out dx, al

	inc dx;
	mov al, 0x20
	out dx, al

	xor ecx, ecx
	mov cl, bl

	.read:
		push cx
		call .waits
		call .reads
		pop cx
		loop .read
	ret

	.waits:
		mov dx, 0x1f7
		.check:
			in al, dx
			jmp $+2
			jmp $+2
			jmp $+2
			and al, 0b1000_1000
			cmp al, 0b0000_1000
			jnz .check
		ret

	.reads:
		mov dx, 0x1f0
		mov cx, 256
		.readw:
			in ax, dx
			jmp $+2
			jmp $+2
			jmp $+2
			mov [edi], ax
			add edi, 2
			loop .readw
		ret

print:
	mov ah, 0x0e
.next:
	mov al, [si]
	cmp al, 0
	jz .done
	int 0x10
	inc si
	jmp .next
.done:
	ret

booting:
	db "Booting Onix...", 10, 13, 0; \n\r

error:
	mov si, .msg
	call print
	hlt
	jmp $
	.msg db "Booting Error!!!", 10, 13, 0

; 填充0
times 510 - ($ - $$) db 0

; 主引导扇区最后两个字节
db 0x55, 0xaa
