BUILD:=../build
SRC:=.

ENTRYPOINT:=0x10000

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

# $(BUILD)/lib/%.o: $(SRC)/lib/%.c
# 	$(shell mkdir -p $(dir $@))
# 	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/start.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/io.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/kernel/console.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/kernel/printk.o \
	$(BUILD)/kernel/assert.o \
	$(BUILD)/kernel/debug.o \
	$(BUILD)/kernel/global.o \
	$(BUILD)/kernel/schedule.o \
	$(BUILD)/kernel/task.o \
	$(BUILD)/kernel/interrupt.o \
	$(BUILD)/kernel/handler.o
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -Ttext $(ENTRYPOINT)

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
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
	../bochs/bin/bochs -q -f ../bochs/bochsrc

.PHONY: qemu
qemu: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda $<

.PHONY: qemug
qemug: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	qemu-system-i386 \
	-s -S \
	-m 32M \
	-boot c \
	-hda $<

test: $(BUILD)/os.img