# Makefile para Micro-Kernel x86-64 - Fase 4.5 / 5

CROSS := $(shell command -v x86_64-elf-gcc 2>/dev/null)

ifdef CROSS
    CC = x86_64-elf-gcc
    LD = x86_64-elf-ld
else
    CC = gcc
    LD = ld
endif

AS = nasm

CFLAGS = -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
         -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
         -Wall -Wextra -O2 -g

ASFLAGS = -f elf64 -g
LDFLAGS = -nostdlib -static -T linker.ld

ASM_SOURCES = boot/multiboot2.asm \
              boot/boot.asm \
              kernel/isr.asm \
              kernel/syscall_entry.asm \
              proc/switch.asm \
              proc/usermode.asm

C_SOURCES = kernel.c \
            drivers/console.c \
            drivers/keyboard.c \
            drivers/vfs.c \
            drivers/disk.c \
            drivers/partition.c \
            drivers/mesafs.c \
            kernel/pic.c \
            kernel/tss.c \
            kernel/idt.c \
            kernel/syscall.c \
            kernel/uaccess.c \
            mm/mm.c \
            mm/kheap.c \
            proc/proc.c \
            sched/sched.c \
            ipc/ipc.c \
            elf/elf.c \
            user/user_prog.c \
            user/shell.c \
            user/ipc_test.c

ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
C_OBJECTS   = $(C_SOURCES:.c=.o)
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

.PHONY: all clean run debug iso dirs

all: dirs kernel.bin

dirs:
	@mkdir -p boot kernel drivers mm proc sched user include ipc elf iso/boot/grub

kernel.bin: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

iso: kernel.bin
	@mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "MicroKernel" { multiboot2 /boot/kernel.bin; boot; }' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o kernel.iso iso 2>/dev/null
	@echo "ISO created"

run: iso
	qemu-system-x86_64 -cdrom kernel.iso -m 128M

debug: iso
	qemu-system-x86_64 -cdrom kernel.iso -m 128M -d int,cpu_reset -no-reboot

clean:
	rm -f boot/*.o kernel/*.o drivers/*.o mm/*.o proc/*.o sched/*.o user/*.o ipc/*.o elf/*.o *.o
	rm -f kernel.bin kernel.iso
	rm -rf iso
	@echo "Clean complete"