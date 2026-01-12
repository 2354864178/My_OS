include Makefile

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

.PHONY: bochsb
bochsb: $(BUILD)/kernel.iso
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc.grub -unlock -debugger

QEMU+= -drive file=$(BUILD)/kernel.iso,media=cdrom,if=ide # 光盘镜像

QEMU_CDROM:= -boot d

.PHONY: qemub
qemub: $(BUILD)/kernel.iso $(IMAGES)
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) \
	$(QEMU_CDROM) \
# 	$(QEMU_DEBUG)

.PHONY:cdrom
cdrom: $(BUILD)/kernel.iso $(IMAGES)