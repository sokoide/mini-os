# Makefile for Day 03 Completed - C-based VGA with split boot

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
	@echo "✅ Day 03 Completed: ビルド完了"
	@echo "🚀 実行: make run"

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
	$(QEMU) -fda $(OS_IMG) -boot a

clean:
	rm -f $(BOOTBIN) kernel_entry.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
