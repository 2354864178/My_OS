.PHONY: dtb
dtb: $(DTB) $(ISO_DTB)

.PHONY: bochsg
bochsg: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc -debugger

.PHONY: bochs
bochs: $(BUILD)/os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -q -f ../bochs/bochsrc -unlock

QEMU:= qemu-system-i386  				# 使用i386架构模拟器
QEMU+= -m 32M 							# 分配32M内存
QEMU+= -audiodev pa,id=hda				# 使用PulseAudio音频驱动
QEMU+= -machine pcspk-audiodev=hda 		# 使用PC扬声器并连接到音频驱动
QEMU+= -rtc base=localtime 				# 使用本地时间作为RTC时间
QEMU+= -serial stdio 					# 将串口输出重定向到标准输入输出
QEMU+= -device nvme,drive=nvme0,serial=nvme0 	# 使用NVMe设备模拟主磁盘
QEMU+= -device nvme,drive=nvme1,serial=nvme1 	# 使用NVMe设备模拟从磁盘
# QEMU+= -drive file=$(BUILD)/os.img,if=ide,index=0,media=disk,format=raw		# 主磁盘
# QEMU+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw	# 从磁盘
QEMU+= -drive file=$(BUILD)/os.img,if=none,id=nvme0,index=0,media=disk,format=raw		# 主磁盘
QEMU+= -drive file=$(BUILD)/slave.img,if=none,index=1,id=nvme1,media=disk,format=raw	# 从磁盘


QEMU_DISK:= -boot c

QEMU_DEBUG:= -s -S

# # 已失效 请看cdrok.mk中的qemub和qemubg
# .PHONY: qemu 		# 已失效 使用qemub代替
# qemu:
# 	$(MAKE) -B $(IMAGES)
# 	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
# 	$(QEMU) $(QEMU_DISK)

# .PHONY: qemug 	# 已失效 使用qemubg代替
# qemug:
# 	$(MAKE) -B $(IMAGES)
# 	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0"  \
# 	$(QEMU) $(QEMU_DISK) $(QEMU_DEBUG)

test: $(BUILD)/os.img