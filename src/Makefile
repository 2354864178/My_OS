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
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/bitmap.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/list.o
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
	$(BUILD)/system.map
	yes | ../bochs/bin/bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc

$(BUILD)/kernel.iso : $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
# 检查多重引导2兼容性
	grub-file --is-x86-multiboot2 $<
# 创建目录
	mkdir -p $(BUILD)/iso/boot/grub
# 复制内核文件
	cp $< $(BUILD)/iso/boot	
# 复制grub配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub/	
# 生成ISO文件
	grub-mkrescue -o $@ $(BUILD)/iso	

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

.PHONY: bochsb
bochsb: $(BUILD)/kernel.iso
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc.grub -unlock -debugger

QEMU = qemu-system-i386 \
	-m 32M \
	-audiodev pa,id=hda \
	-machine pcspk-audiodev=hda \
	-rtc base=localtime \

QEMU_DISK:= -boot c \
	-drive file=$(BUILD)/os.img,if=ide,index=0,media=disk,format=raw

QEMU_CDROW:= -boot d \
	-drive file=$(BUILD)/kernel.iso,media=cdrom

QEMU_DEBUG:= -s -S

.PHONY: qemu
qemu: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug
qemug: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

.PHONY: qemub
qemub: $(BUILD)/kernel.iso
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_CDROW) #$(QEMU_DEBUG)

test: $(BUILD)/os.img