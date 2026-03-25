BUILD:=../build
SRC:=.

DTC?=dtc
DTS:=$(SRC)/devicetree/device.dts
DTB:=$(BUILD)/device.dtb
ISO_DTB:=$(BUILD)/iso/boot/device.dtb
DTB_OBJ:=$(BUILD)/device_dtb.o

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

$(DTB): $(DTS)
	$(shell mkdir -p $(dir $@))
	$(DTC) -I dts -O dtb -o $@ $<

$(DTB_OBJ): $(DTB)
	$(shell mkdir -p $(dir $@))
	objcopy -I binary -O elf32-i386 -B i386 $< $@

$(ISO_DTB): $(DTB)
	$(shell mkdir -p $(dir $@))
	cp $< $@

LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS:=$(strip ${LDFLAGS})

$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/start.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/io.o \
	$(BUILD)/kernel/device.o \
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
	$(BUILD)/kernel/pci.o \
	$(BUILD)/kernel/nvme.o \
	$(BUILD)/kernel/ide.o \
	$(BUILD)/kernel/buffer.o \
	$(BUILD)/kernel/devicetree.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/bitmap.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/list.o \
	$(BUILD)/lib/fifo.o \
	$(BUILD)/lib/printf.o \
	$(BUILD)/fs/super.o \
	$(BUILD)/fs/bmap.o \
	$(BUILD)/fs/inode.o \
	$(DTB_OBJ)

$(BUILD)/kernel.bin: | $(DTB)
	$(shell mkdir -p $(dir $@))
	ld $(LDFLAGS) -o $@ $^

$(BUILD)/system.bin: $(BUILD)/kernel.bin	# 生成系统二进制文件
	objcopy -O binary $< $@	

$(BUILD)/system.map: $(BUILD)/kernel.bin	# 生成系统符号表
	nm $< | sort > $@

include utils/cdrok.mk
include utils/image.mk
include utils/cmd.mk

.PHONY: clean
clean:
	rm -rf ../build/*.bin
	rm -rf ../build/*.img