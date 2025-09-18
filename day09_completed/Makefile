# Makefile for Day 09 Completed - プリエンプティブ・ラウンドロビン

AS   = nasm
CC   = i686-elf-gcc
LD   = i686-elf-ld
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-i386

CFLAGS = -ffreestanding -m32 -nostdlib -fno-stack-protector -fno-pic -O2 -Wall -Wextra

BOOT    = boot/boot.s
ENTRY   = boot/kernel_entry.s
INTRASM = boot/interrupt.s

CTXASM  = boot/context_switch.s
KERNELC = kernel.c

BOOTBIN = boot.bin
KELF    = kernel.elf
KBIN    = kernel.bin
OS_IMG  = os.img

all: $(OS_IMG)
	@echo "✅ Day 09 Completed: プリエンプション ビルド完了"
	@echo "🚀 実行: make run"

$(BOOTBIN): $(BOOT)
	$(AS) -f bin $(BOOT) -o $(BOOTBIN)

kernel_entry.o: $(ENTRY)
	$(AS) -f elf32 $(ENTRY) -o $@

interrupt.o: $(INTRASM)
	$(AS) -f elf32 $(INTRASM) -o $@

context_switch.o: $(CTXASM)
	$(AS) -f elf32 $(CTXASM) -o $@


kernel.o: $(KERNELC) vga.h io.h
	$(CC) $(CFLAGS) -c $(KERNELC) -o $@

$(KELF): kernel_entry.o interrupt.o context_switch.o kernel.o
	$(LD) -m elf_i386 -Ttext 0x00010000 -e kernel_entry -o $(KELF) kernel_entry.o interrupt.o context_switch.o kernel.o

$(KBIN): $(KELF)
	$(OBJCOPY) -O binary $(KELF) $(KBIN)

$(OS_IMG): $(BOOTBIN) $(KBIN)
	cat $(BOOTBIN) $(KBIN) > $(OS_IMG)
	truncate -s 1440K $(OS_IMG)

run: $(OS_IMG)
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=floppy -boot a -serial stdio

clean:
	rm -f $(BOOTBIN) kernel_entry.o interrupt.o context_switch.o kernel.o $(KELF) $(KBIN) $(OS_IMG)

.PHONY: all run clean
