# Makefile for Day 04 Completed - Cベースのシリアル + VGA（boot分割）

AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 04 Completed: Cベース構成でビルド完了"
	@echo "🚀 実行: make run  （シリアルは端末に表示されます）"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)
	truncate -s 1440K $(OS_IMG)

run: $(OS_IMG)
	@echo "🚀 QEMUで起動（-serial stdio）..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio

debug: $(OS_IMG)
	@echo "🔍 モニタ付きで起動..."
	$(QEMU) -fda $(OS_IMG) -boot a -serial stdio -monitor tcp:127.0.0.1:4444,server,nowait

clean:
	@echo "🧹 クリーンアップ..."
	rm -f $(BOOTBIN) kernel_entry.o kernel.o $(KELF) $(KBIN) $(OS_IMG)
	@echo "✅ 完了"

.PHONY: all run debug clean
