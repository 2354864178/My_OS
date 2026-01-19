BUILD:=../build
SRC:=.

MULTIBOOT2:=0x10000 # 多重引导2头部加载地址
ENTRYPOINT:=0x10040 # 内核入口地址

CFLAGS:= -m32 \
		-fno-builtin \
		-nostdinc \
		-fno-pic \
		-fno-pie \
		-nostdlib \
		-fno-stack-protector 
CFLAGS:=$(strip ${CFLAGS})

DEBUG:= -g
INCLUDE:=-I$(SRC)/include

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(BUILD)/%.o: $(SRC)/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/%.o: $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS:=$(strip ${LDFLAGS})

$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/start.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/io.o \
	$(BUILD)/kernel/console.o \
	$(BUILD)/kernel/printk.o \
	$(BUILD)/kernel/assert.o \
	$(BUILD)/kernel/debug.o \
	$(BUILD)/kernel/global.o \
	$(BUILD)/kernel/schedule.o \
	$(BUILD)/kernel/task.o \
	$(BUILD)/kernel/interrupt.o \
	$(BUILD)/kernel/handler.o \
	$(BUILD)/kernel/clock.o \
	$(BUILD)/kernel/time.o \
	$(BUILD)/kernel/rtc.o \
	$(BUILD)/kernel/memory.o \
	$(BUILD)/kernel/gate.o \
	$(BUILD)/kernel/thread.o \
	$(BUILD)/kernel/mutex.o \
	$(BUILD)/kernel/keyboard.o \
	$(BUILD)/kernel/arena.o \
	$(BUILD)/kernel/ide.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/bitmap.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/list.o \
	$(BUILD)/lib/fifo.o \
	$(BUILD)/lib/printf.o
	$(shell mkdir -p $(dir $@))
	ld $(LDFLAGS) -o $@ $^

$(BUILD)/system.bin: $(BUILD)/kernel.bin	# 生成系统二进制文件
	objcopy -O binary $< $@	

$(BUILD)/system.map: $(BUILD)/kernel.bin	# 生成系统符号表
	nm $< | sort > $@

$(BUILD)/os.img: \
	$(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map \
	$(SRC)/utils/os.sfdisk
	yes | ../bochs/bin/bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc
	sfdisk $@ < $(SRC)/utils/os.sfdisk

$(BUILD)/slave.img:
	$(shell mkdir -p $(dir $@))
	yes | ../bochs/bin/bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@

IMAGES:= $(BUILD)/os.img $(BUILD)/slave.img

.PHONY: clean
clean:
	rm -rf *.bin
	rm -rf *.img

.PHONY: bochsg
bochsg: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc -debugger

.PHONY: bochs
bochs: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc -unlock

QEMU:= qemu-system-i386 				# 使用i386架构模拟器
QEMU+= -m 32M 							# 分配32M内存
QEMU+= -audiodev pa,id=hda				# 使用PulseAudio音频驱动
QEMU+= -machine pcspk-audiodev=hda 		# 使用PC扬声器并连接到音频驱动
QEMU+= -rtc base=localtime 				# 使用本地时间作为RTC时间
QEMU+= -serial stdio 					# 将串口输出重定向到标准输入输出
QEMU+= -drive file=$(BUILD)/os.img,if=ide,index=0,media=disk,format=raw
QEMU+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw

QEMU_DISK:= -boot c

QEMU_DEBUG:= -s -S

.PHONY: qemu
qemu:
	$(MAKE) -B $(IMAGES)
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug:
	$(MAKE) -B $(IMAGES)
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

test: $(BUILD)/os.img