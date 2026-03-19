$(BUILD)/os.img: \
	$(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map \
	$(SRC)/utils/os.sfdisk

	yes | ../bochs/bin/bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc

	test -n "$$(find $(BUILD)/system.bin -size -100k)"
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc
	sfdisk $@ < $(SRC)/utils/os.sfdisk
	set -e; \
	loopdev=$$(sudo losetup --find --show --partscan $@); \
	trap 'sudo umount /mnt 2>/dev/null || true; sudo losetup -d "$$loopdev" 2>/dev/null || true' EXIT; \
	sudo mkfs.minix -1 -n 14 "$${loopdev}p1"; \
	sudo mount "$${loopdev}p1" /mnt; \
	sudo chown ${USER} /mnt; \
	mkdir -p /mnt/home; \
	mkdir -p /mnt/d1/d2/d3/d4; \
	echo "os Hello, World!" > /mnt/hello.txt; \
	echo "os Hello, World!" > /mnt/home/hello.txt; \
	sudo umount /mnt; \
	sudo losetup -d "$$loopdev"; \
	trap - EXIT

$(BUILD)/slave.img: $(SRC)/utils/slave.sfdisk
	$(shell mkdir -p $(dir $@))
	yes | ../bochs/bin/bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@
	sfdisk $@ < $(SRC)/utils/slave.sfdisk

	set -e; \
	loopdev=$$(sudo losetup --find --show --partscan $@); \
	trap 'sudo umount /mnt 2>/dev/null || true; sudo losetup -d "$$loopdev" 2>/dev/null || true' EXIT; \
	for i in 1 2 3 4 5; do [ -b "$${loopdev}p1" ] && break; sleep 0.2; done; \
	test -b "$${loopdev}p1"; \
	sudo mkfs.minix -1 -n 14 "$${loopdev}p1"; \
	sudo mount "$${loopdev}p1" /mnt; \
	sudo chown ${USER} /mnt; \
	echo "slave Hello, World!" > /mnt/hello.txt; \
	sudo umount /mnt; \
	sudo losetup -d "$$loopdev"; \
	trap - EXIT

# 生成系统二进制文件和符号表
.PHONY: mount0
mount0: $(BUILD)/os.img
	set -e; \
	loopdev=$$(sudo losetup --find --show --partscan $<); \
	echo "$$loopdev" > $(BUILD)/.mount0.loop; \
	sudo mount "$${loopdev}p1" /mnt; \
	sudo chown ${USER} /mnt

# 卸载系统镜像
.PHONY: umount0
umount0:
	set -e; \
	loopdev=$$(cat $(BUILD)/.mount0.loop 2>/dev/null || true); \
	sudo umount /mnt 2>/dev/null || true; \
	if [ -n "$$loopdev" ]; then sudo losetup -d "$$loopdev" 2>/dev/null || true; fi; \
	rm -f $(BUILD)/.mount0.loop

# 生成系统镜像和符号表
.PHONY: mount1
mount1: $(BUILD)/slave.img
	set -e; \
	loopdev=$$(sudo losetup --find --show --partscan $<); \
	echo "$$loopdev" > $(BUILD)/.mount1.loop; \
	sudo mount "$${loopdev}p1" /mnt; \
	sudo chown ${USER} /mnt

.PHONY: umount1
umount1:
	set -e; \
	loopdev=$$(cat $(BUILD)/.mount1.loop 2>/dev/null || true); \
	sudo umount /mnt 2>/dev/null || true; \
	if [ -n "$$loopdev" ]; then sudo losetup -d "$$loopdev" 2>/dev/null || true; fi; \
	rm -f $(BUILD)/.mount1.loop

IMAGES:= $(BUILD)/os.img $(BUILD)/slave.img

image: $(IMAGES)