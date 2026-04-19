$(BUILD)/kernel.iso : $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
# 检查多重引导2兼容性
	grub-file --is-x86-multiboot2 $<
# 检查生成ISO所需工具
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "错误: 未找到 grub-mkrescue，请先安装 grub-pc-bin 后再执行 make qemub"; exit 1; }
	@command -v xorriso >/dev/null 2>&1 || { echo "错误: 未找到 xorriso，请先安装后再执行 make qemub"; exit 1; }
	@command -v mformat >/dev/null 2>&1 || { echo "错误: 未找到 mformat，请先安装 mtools 后再执行 make qemub"; exit 1; }
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

QEMU_CDROM:= -drive file=$(BUILD)/kernel.iso,media=cdrom,if=ide # 光盘镜像

QEMU_CDROM_BOOT:= -boot d

.PHONY: qemub clean image
qemub: clean image $(BUILD)/kernel.iso $(IMAGES)
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) $(QEMU_CDROM) $(QEMU_CDROM_BOOT)

.PHONY: qemubg 
qemubg: $(BUILD)/kernel.iso $(IMAGES)
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
	$(QEMU) \
	$(QEMU_CDROM) \
	$(QEMU_CDROM_BOOT) \
	$(QEMU_DEBUG)

.PHONY:cdrom
cdrom: $(BUILD)/kernel.iso $(IMAGES)