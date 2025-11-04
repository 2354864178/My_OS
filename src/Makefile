%.bin: %.asm
	nasm -f bin $< -o $@

os.img: boot.bin loader.bin
	yes | ../bochs/bin/bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat os.img
	dd if=boot.bin of=os.img bs=512 count=1 conv=notrunc
	dd if=loader.bin of=os.img bs=512 count=4 seek=2 conv=notrunc

.PHONY: clean
clean:
	rm -rf *.bin
	rm -rf *.img

.PHONY: bochs
bochs: os.img
	LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libpthread.so.0" \
	../bochs/bin/bochs -f bochsrc -debugger
