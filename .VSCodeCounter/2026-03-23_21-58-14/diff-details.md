# Diff Details

Date : 2026-03-23 21:58:14

Directory /home/ysy/Code/My_OS

Total : 161 files,  -380 codes, 32 comments, -105 blanks, all -453 lines

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [bochs/share/doc/bochs/slirp.conf](/bochs/share/doc/bochs/slirp.conf) | Properties | 0 | 49 | 8 | 57 |
| [bochs/share/doc/bochs/vnet.conf](/bochs/share/doc/bochs/vnet.conf) | Properties | 0 | 21 | 4 | 25 |
| [build/debug.log](/build/debug.log) | Log | 0 | 0 | 1 | 1 |
| [build/iso/boot/grub/grub.cfg](/build/iso/boot/grub/grub.cfg) | Properties | 5 | 0 | 1 | 6 |
| [build/serial.log](/build/serial.log) | Log | 6 | 0 | 5 | 11 |
| [docs/arm\_porting.md](/docs/arm_porting.md) | Markdown | 86 | 0 | 18 | 104 |
| [src/Makefile](/src/Makefile) | Makefile | 93 | 0 | 17 | 110 |
| [src/bx\_enh\_dbg.ini](/src/bx_enh_dbg.ini) | Ini | 26 | 0 | 1 | 27 |
| [src/include/onix/apic.h](/src/include/onix/apic.h) | C++ | 41 | 8 | 17 | 66 |
| [src/include/onix/arena.h](/src/include/onix/arena.h) | C | 20 | 2 | 7 | 29 |
| [src/include/onix/assert.h](/src/include/onix/assert.h) | C | 10 | 0 | 4 | 14 |
| [src/include/onix/bitmap.h](/src/include/onix/bitmap.h) | C++ | 15 | 0 | 9 | 24 |
| [src/include/onix/buffer.h](/src/include/onix/buffer.h) | C++ | 23 | 0 | 5 | 28 |
| [src/include/onix/console.h](/src/include/onix/console.h) | C | 6 | 1 | 4 | 11 |
| [src/include/onix/debug.h](/src/include/onix/debug.h) | C | 6 | 1 | 6 | 13 |
| [src/include/onix/device.h](/src/include/onix/device.h) | C++ | 60 | 6 | 13 | 79 |
| [src/include/onix/devicetree.h](/src/include/onix/devicetree.h) | C++ | 9 | 7 | 8 | 24 |
| [src/include/onix/fifo.h](/src/include/onix/fifo.h) | C++ | 16 | 0 | 4 | 20 |
| [src/include/onix/global.h](/src/include/onix/global.h) | C | 78 | 5 | 12 | 95 |
| [src/include/onix/ide.h](/src/include/onix/ide.h) | C++ | 63 | 2 | 11 | 76 |
| [src/include/onix/interrupt.h](/src/include/onix/interrupt.h) | C | 43 | 27 | 10 | 80 |
| [src/include/onix/io.h](/src/include/onix/io.h) | C++ | 10 | 0 | 4 | 14 |
| [src/include/onix/list.h](/src/include/onix/list.h) | C++ | 30 | 2 | 12 | 44 |
| [src/include/onix/memory.h](/src/include/onix/memory.h) | C | 50 | 2 | 10 | 62 |
| [src/include/onix/mmio.h](/src/include/onix/mmio.h) | C++ | 37 | 7 | 9 | 53 |
| [src/include/onix/multiboot2.h](/src/include/onix/multiboot2.h) | C | 29 | 3 | 9 | 41 |
| [src/include/onix/mutex.h](/src/include/onix/mutex.h) | C | 20 | 3 | 8 | 31 |
| [src/include/onix/nvme.h](/src/include/onix/nvme.h) | C++ | 85 | 5 | 19 | 109 |
| [src/include/onix/onix.h](/src/include/onix/onix.h) | C | 5 | 0 | 3 | 8 |
| [src/include/onix/pci.h](/src/include/onix/pci.h) | C++ | 12 | 3 | 8 | 23 |
| [src/include/onix/printk.h](/src/include/onix/printk.h) | C++ | 4 | 0 | 2 | 6 |
| [src/include/onix/rtc.h](/src/include/onix/rtc.h) | C++ | 33 | 4 | 9 | 46 |
| [src/include/onix/stdarg.h](/src/include/onix/stdarg.h) | C++ | 7 | 0 | 4 | 11 |
| [src/include/onix/stdio.h](/src/include/onix/stdio.h) | C | 7 | 0 | 3 | 10 |
| [src/include/onix/stdlib.h](/src/include/onix/stdlib.h) | C++ | 16 | 0 | 9 | 25 |
| [src/include/onix/string.h](/src/include/onix/string.h) | C++ | 18 | 0 | 4 | 22 |
| [src/include/onix/syscall.h](/src/include/onix/syscall.h) | C++ | 28 | 0 | 6 | 34 |
| [src/include/onix/task.h](/src/include/onix/task.h) | C++ | 81 | 3 | 14 | 98 |
| [src/include/onix/time.h](/src/include/onix/time.h) | C++ | 20 | 0 | 5 | 25 |
| [src/include/onix/types.h](/src/include/onix/types.h) | C | 38 | 3 | 11 | 52 |
| [src/kernel/arena.c](/src/kernel/arena.c) | C | 86 | 18 | 22 | 126 |
| [src/kernel/assert.c](/src/kernel/assert.c) | C | 31 | 3 | 8 | 42 |
| [src/kernel/buffer.c](/src/kernel/buffer.c) | C | 129 | 19 | 32 | 180 |
| [src/kernel/clock.c](/src/kernel/clock.c) | C | 112 | 7 | 29 | 148 |
| [src/kernel/console.c](/src/kernel/console.c) | C | 261 | 33 | 38 | 332 |
| [src/kernel/debug.c](/src/kernel/debug.c) | C | 11 | 0 | 2 | 13 |
| [src/kernel/device.c](/src/kernel/device.c) | C | 132 | 23 | 24 | 179 |
| [src/kernel/devicetree.c](/src/kernel/devicetree.c) | C | 137 | 7 | 22 | 166 |
| [src/kernel/gate.c](/src/kernel/gate.c) | C | 59 | 4 | 13 | 76 |
| [src/kernel/global.c](/src/kernel/global.c) | C | 68 | 3 | 13 | 84 |
| [src/kernel/ide.c](/src/kernel/ide.c) | C | 384 | 43 | 64 | 491 |
| [src/kernel/interrupt.c](/src/kernel/interrupt.c) | C | 255 | 38 | 51 | 344 |
| [src/kernel/keyboard.c](/src/kernel/keyboard.c) | C | 372 | 13 | 48 | 433 |
| [src/kernel/main.c](/src/kernel/main.c) | C | 37 | 1 | 5 | 43 |
| [src/kernel/memory.c](/src/kernel/memory.c) | C | 432 | 50 | 91 | 573 |
| [src/kernel/mutex.c](/src/kernel/mutex.c) | C | 62 | 8 | 18 | 88 |
| [src/kernel/nvme.c](/src/kernel/nvme.c) | C | 472 | 60 | 99 | 631 |
| [src/kernel/pci.c](/src/kernel/pci.c) | C | 84 | 13 | 20 | 117 |
| [src/kernel/printk.c](/src/kernel/printk.c) | C | 15 | 0 | 3 | 18 |
| [src/kernel/rtc.c](/src/kernel/rtc.c) | C | 91 | 10 | 23 | 124 |
| [src/kernel/task.c](/src/kernel/task.c) | C | 331 | 26 | 84 | 441 |
| [src/kernel/thread.c](/src/kernel/thread.c) | C | 41 | 3 | 6 | 50 |
| [src/kernel/time.c](/src/kernel/time.c) | C | 139 | 10 | 27 | 176 |
| [src/lib/bitmap.c](/src/lib/bitmap.c) | C | 55 | 8 | 19 | 82 |
| [src/lib/fifo.c](/src/lib/fifo.c) | C | 38 | 0 | 7 | 45 |
| [src/lib/list.c](/src/lib/list.c) | C | 97 | 13 | 23 | 133 |
| [src/lib/printf.c](/src/lib/printf.c) | C | 14 | 0 | 8 | 22 |
| [src/lib/stdlib.c](/src/lib/stdlib.c) | C | 45 | 6 | 10 | 61 |
| [src/lib/string.c](/src/lib/string.c) | C | 176 | 2 | 16 | 194 |
| [src/lib/syscall.c](/src/lib/syscall.c) | C | 70 | 2 | 16 | 88 |
| [src/lib/vsprintf.c](/src/lib/vsprintf.c) | C | 285 | 81 | 50 | 416 |
| [src/utils/cdrok.mk](/src/utils/cdrok.mk) | Makefile | 24 | 5 | 6 | 35 |
| [src/utils/cmd.mk](/src/utils/cmd.mk) | Makefile | 23 | 13 | 9 | 45 |
| [src/utils/grub.cfg](/src/utils/grub.cfg) | Properties | 5 | 0 | 1 | 6 |
| [src/utils/image.mk](/src/utils/image.mk) | Makefile | 71 | 3 | 10 | 84 |
| [test/makefile](/test/makefile) | Makefile | 14 | 8 | 4 | 26 |
| [test/param.c](/test/param.c) | C | 10 | 0 | 2 | 12 |
| [test/types.c](/test/types.c) | C | 27 | 0 | 5 | 32 |
| [/home/ysy/code/Os\_wrtie/bochs/share/doc/bochs/slirp.conf](//home/ysy/code/Os_wrtie/bochs/share/doc/bochs/slirp.conf) | Properties | 0 | -49 | -8 | -57 |
| [/home/ysy/code/Os\_wrtie/bochs/share/doc/bochs/vnet.conf](//home/ysy/code/Os_wrtie/bochs/share/doc/bochs/vnet.conf) | Properties | 0 | -21 | -4 | -25 |
| [/home/ysy/code/Os\_wrtie/build/debug.log](//home/ysy/code/Os_wrtie/build/debug.log) | Log | 0 | 0 | -1 | -1 |
| [/home/ysy/code/Os\_wrtie/build/iso/boot/grub/grub.cfg](//home/ysy/code/Os_wrtie/build/iso/boot/grub/grub.cfg) | Properties | -5 | 0 | -1 | -6 |
| [/home/ysy/code/Os\_wrtie/build/serial.log](//home/ysy/code/Os_wrtie/build/serial.log) | Log | -6 | 0 | -5 | -11 |
| [/home/ysy/code/Os\_wrtie/docs/arm\_porting.md](//home/ysy/code/Os_wrtie/docs/arm_porting.md) | Markdown | -86 | 0 | -18 | -104 |
| [/home/ysy/code/Os\_wrtie/src/Makefile](//home/ysy/code/Os_wrtie/src/Makefile) | Makefile | -138 | -2 | -30 | -170 |
| [/home/ysy/code/Os\_wrtie/src/boot/boot.asm](//home/ysy/code/Os_wrtie/src/boot/boot.asm) | Assembler file | -102 | 0 | -26 | -128 |
| [/home/ysy/code/Os\_wrtie/src/boot/loader.asm](//home/ysy/code/Os_wrtie/src/boot/loader.asm) | Assembler file | -154 | 0 | -44 | -198 |
| [/home/ysy/code/Os\_wrtie/src/bx\_enh\_dbg.ini](//home/ysy/code/Os_wrtie/src/bx_enh_dbg.ini) | Ini | -26 | 0 | -1 | -27 |
| [/home/ysy/code/Os\_wrtie/src/cdrok.mk](//home/ysy/code/Os_wrtie/src/cdrok.mk) | Makefile | -28 | -5 | -7 | -40 |
| [/home/ysy/code/Os\_wrtie/src/devicetree/device.dts](//home/ysy/code/Os_wrtie/src/devicetree/device.dts) | DeviceTree | -69 | -8 | -11 | -88 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/apic.h](//home/ysy/code/Os_wrtie/src/include/onix/apic.h) | C++ | -41 | -8 | -17 | -66 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/arena.h](//home/ysy/code/Os_wrtie/src/include/onix/arena.h) | C | -20 | -2 | -7 | -29 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/assert.h](//home/ysy/code/Os_wrtie/src/include/onix/assert.h) | C | -10 | 0 | -4 | -14 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/bitmap.h](//home/ysy/code/Os_wrtie/src/include/onix/bitmap.h) | C++ | -15 | 0 | -9 | -24 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/console.h](//home/ysy/code/Os_wrtie/src/include/onix/console.h) | C | -6 | -1 | -4 | -11 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/debug.h](//home/ysy/code/Os_wrtie/src/include/onix/debug.h) | C | -6 | -1 | -6 | -13 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/device.h](//home/ysy/code/Os_wrtie/src/include/onix/device.h) | C++ | -57 | -6 | -11 | -74 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/devicetree.h](//home/ysy/code/Os_wrtie/src/include/onix/devicetree.h) | C++ | -9 | -7 | -8 | -24 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/fifo.h](//home/ysy/code/Os_wrtie/src/include/onix/fifo.h) | C++ | -16 | 0 | -4 | -20 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/global.h](//home/ysy/code/Os_wrtie/src/include/onix/global.h) | C | -78 | -5 | -12 | -95 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/ide.h](//home/ysy/code/Os_wrtie/src/include/onix/ide.h) | C++ | -63 | -2 | -11 | -76 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/interrupt.h](//home/ysy/code/Os_wrtie/src/include/onix/interrupt.h) | C | -44 | -26 | -10 | -80 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/io.h](//home/ysy/code/Os_wrtie/src/include/onix/io.h) | C++ | -10 | 0 | -4 | -14 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/list.h](//home/ysy/code/Os_wrtie/src/include/onix/list.h) | C++ | -30 | -2 | -12 | -44 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/memory.h](//home/ysy/code/Os_wrtie/src/include/onix/memory.h) | C | -50 | -2 | -11 | -63 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/mmio.h](//home/ysy/code/Os_wrtie/src/include/onix/mmio.h) | C++ | -37 | -7 | -9 | -53 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/multiboot2.h](//home/ysy/code/Os_wrtie/src/include/onix/multiboot2.h) | C | -29 | -3 | -9 | -41 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/mutex.h](//home/ysy/code/Os_wrtie/src/include/onix/mutex.h) | C | -20 | -3 | -8 | -31 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/nvme.h](//home/ysy/code/Os_wrtie/src/include/onix/nvme.h) | C++ | -73 | -4 | -16 | -93 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/onix.h](//home/ysy/code/Os_wrtie/src/include/onix/onix.h) | C | -5 | 0 | -3 | -8 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/pci.h](//home/ysy/code/Os_wrtie/src/include/onix/pci.h) | C++ | -12 | -3 | -8 | -23 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/printk.h](//home/ysy/code/Os_wrtie/src/include/onix/printk.h) | C++ | -4 | 0 | -2 | -6 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/rtc.h](//home/ysy/code/Os_wrtie/src/include/onix/rtc.h) | C++ | -33 | -4 | -9 | -46 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/stdarg.h](//home/ysy/code/Os_wrtie/src/include/onix/stdarg.h) | C++ | -7 | 0 | -4 | -11 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/stdio.h](//home/ysy/code/Os_wrtie/src/include/onix/stdio.h) | C | -7 | 0 | -3 | -10 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/stdlib.h](//home/ysy/code/Os_wrtie/src/include/onix/stdlib.h) | C++ | -16 | 0 | -9 | -25 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/string.h](//home/ysy/code/Os_wrtie/src/include/onix/string.h) | C++ | -18 | 0 | -4 | -22 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/syscall.h](//home/ysy/code/Os_wrtie/src/include/onix/syscall.h) | C++ | -28 | 0 | -6 | -34 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/task.h](//home/ysy/code/Os_wrtie/src/include/onix/task.h) | C++ | -81 | -3 | -14 | -98 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/time.h](//home/ysy/code/Os_wrtie/src/include/onix/time.h) | C++ | -20 | 0 | -5 | -25 |
| [/home/ysy/code/Os\_wrtie/src/include/onix/types.h](//home/ysy/code/Os_wrtie/src/include/onix/types.h) | C | -38 | -3 | -11 | -52 |
| [/home/ysy/code/Os\_wrtie/src/kernel/arena.c](//home/ysy/code/Os_wrtie/src/kernel/arena.c) | C | -86 | -18 | -22 | -126 |
| [/home/ysy/code/Os\_wrtie/src/kernel/assert.c](//home/ysy/code/Os_wrtie/src/kernel/assert.c) | C | -31 | -3 | -8 | -42 |
| [/home/ysy/code/Os\_wrtie/src/kernel/clock.c](//home/ysy/code/Os_wrtie/src/kernel/clock.c) | C | -112 | -7 | -29 | -148 |
| [/home/ysy/code/Os\_wrtie/src/kernel/console.c](//home/ysy/code/Os_wrtie/src/kernel/console.c) | C | -261 | -33 | -38 | -332 |
| [/home/ysy/code/Os\_wrtie/src/kernel/debug.c](//home/ysy/code/Os_wrtie/src/kernel/debug.c) | C | -11 | 0 | -2 | -13 |
| [/home/ysy/code/Os\_wrtie/src/kernel/device.c](//home/ysy/code/Os_wrtie/src/kernel/device.c) | C | -120 | -11 | -15 | -146 |
| [/home/ysy/code/Os\_wrtie/src/kernel/devicetree.c](//home/ysy/code/Os_wrtie/src/kernel/devicetree.c) | C | -137 | -7 | -22 | -166 |
| [/home/ysy/code/Os\_wrtie/src/kernel/gate.c](//home/ysy/code/Os_wrtie/src/kernel/gate.c) | C | -56 | -10 | -14 | -80 |
| [/home/ysy/code/Os\_wrtie/src/kernel/global.c](//home/ysy/code/Os_wrtie/src/kernel/global.c) | C | -68 | -3 | -13 | -84 |
| [/home/ysy/code/Os\_wrtie/src/kernel/handler.asm](//home/ysy/code/Os_wrtie/src/kernel/handler.asm) | Assembler file | -166 | 0 | -33 | -199 |
| [/home/ysy/code/Os\_wrtie/src/kernel/ide.c](//home/ysy/code/Os_wrtie/src/kernel/ide.c) | C | -384 | -43 | -64 | -491 |
| [/home/ysy/code/Os\_wrtie/src/kernel/interrupt.c](//home/ysy/code/Os_wrtie/src/kernel/interrupt.c) | C | -255 | -37 | -51 | -343 |
| [/home/ysy/code/Os\_wrtie/src/kernel/io.asm](//home/ysy/code/Os_wrtie/src/kernel/io.asm) | Assembler file | -73 | 0 | -28 | -101 |
| [/home/ysy/code/Os\_wrtie/src/kernel/keyboard.c](//home/ysy/code/Os_wrtie/src/kernel/keyboard.c) | C | -372 | -13 | -48 | -433 |
| [/home/ysy/code/Os\_wrtie/src/kernel/main.c](//home/ysy/code/Os_wrtie/src/kernel/main.c) | C | -35 | -1 | -4 | -40 |
| [/home/ysy/code/Os\_wrtie/src/kernel/memory.c](//home/ysy/code/Os_wrtie/src/kernel/memory.c) | C | -426 | -50 | -91 | -567 |
| [/home/ysy/code/Os\_wrtie/src/kernel/mutex.c](//home/ysy/code/Os_wrtie/src/kernel/mutex.c) | C | -62 | -8 | -18 | -88 |
| [/home/ysy/code/Os\_wrtie/src/kernel/nvme.c](//home/ysy/code/Os_wrtie/src/kernel/nvme.c) | C | -401 | -49 | -84 | -534 |
| [/home/ysy/code/Os\_wrtie/src/kernel/pci.c](//home/ysy/code/Os_wrtie/src/kernel/pci.c) | C | -84 | -13 | -20 | -117 |
| [/home/ysy/code/Os\_wrtie/src/kernel/printk.c](//home/ysy/code/Os_wrtie/src/kernel/printk.c) | C | -15 | 0 | -3 | -18 |
| [/home/ysy/code/Os\_wrtie/src/kernel/rtc.c](//home/ysy/code/Os_wrtie/src/kernel/rtc.c) | C | -91 | -10 | -23 | -124 |
| [/home/ysy/code/Os\_wrtie/src/kernel/schedule.asm](//home/ysy/code/Os_wrtie/src/kernel/schedule.asm) | Assembler file | -20 | 0 | -9 | -29 |
| [/home/ysy/code/Os\_wrtie/src/kernel/start.asm](//home/ysy/code/Os_wrtie/src/kernel/start.asm) | Assembler file | -53 | 0 | -12 | -65 |
| [/home/ysy/code/Os\_wrtie/src/kernel/task.c](//home/ysy/code/Os_wrtie/src/kernel/task.c) | C | -332 | -25 | -84 | -441 |
| [/home/ysy/code/Os\_wrtie/src/kernel/thread.c](//home/ysy/code/Os_wrtie/src/kernel/thread.c) | C | -42 | -17 | -8 | -67 |
| [/home/ysy/code/Os\_wrtie/src/kernel/time.c](//home/ysy/code/Os_wrtie/src/kernel/time.c) | C | -139 | -10 | -27 | -176 |
| [/home/ysy/code/Os\_wrtie/src/lib/bitmap.c](//home/ysy/code/Os_wrtie/src/lib/bitmap.c) | C | -55 | -8 | -19 | -82 |
| [/home/ysy/code/Os\_wrtie/src/lib/fifo.c](//home/ysy/code/Os_wrtie/src/lib/fifo.c) | C | -38 | 0 | -7 | -45 |
| [/home/ysy/code/Os\_wrtie/src/lib/list.c](//home/ysy/code/Os_wrtie/src/lib/list.c) | C | -97 | -13 | -23 | -133 |
| [/home/ysy/code/Os\_wrtie/src/lib/printf.c](//home/ysy/code/Os_wrtie/src/lib/printf.c) | C | -14 | 0 | -8 | -22 |
| [/home/ysy/code/Os\_wrtie/src/lib/stdlib.c](//home/ysy/code/Os_wrtie/src/lib/stdlib.c) | C | -45 | -6 | -10 | -61 |
| [/home/ysy/code/Os\_wrtie/src/lib/string.c](//home/ysy/code/Os_wrtie/src/lib/string.c) | C | -176 | -2 | -16 | -194 |
| [/home/ysy/code/Os\_wrtie/src/lib/syscall.c](//home/ysy/code/Os_wrtie/src/lib/syscall.c) | C | -70 | -2 | -16 | -88 |
| [/home/ysy/code/Os\_wrtie/src/lib/vsprintf.c](//home/ysy/code/Os_wrtie/src/lib/vsprintf.c) | C | -285 | -81 | -50 | -416 |
| [/home/ysy/code/Os\_wrtie/src/utils/grub.cfg](//home/ysy/code/Os_wrtie/src/utils/grub.cfg) | Properties | -5 | 0 | -1 | -6 |
| [/home/ysy/code/Os\_wrtie/test/call.asm](//home/ysy/code/Os_wrtie/test/call.asm) | Assembler file | -16 | 0 | -9 | -25 |
| [/home/ysy/code/Os\_wrtie/test/makefile](//home/ysy/code/Os_wrtie/test/makefile) | Makefile | -14 | -8 | -4 | -26 |
| [/home/ysy/code/Os\_wrtie/test/param.c](//home/ysy/code/Os_wrtie/test/param.c) | C | -10 | 0 | -2 | -12 |
| [/home/ysy/code/Os\_wrtie/test/param.s](//home/ysy/code/Os_wrtie/test/param.s) | Assembler file | -30 | 0 | -1 | -31 |
| [/home/ysy/code/Os\_wrtie/test/types.c](//home/ysy/code/Os_wrtie/test/types.c) | C | -27 | 0 | -5 | -32 |

[Summary](results.md) / [Details](details.md) / [Diff Summary](diff.md) / Diff Details