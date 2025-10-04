ASM=nasm
CC=gcc

SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy kernel bootloader clean always tools_fat run

all: clean floppy tools_fat

#
# Floppy image
#
floppy: $(BUILD_DIR)/valkyrie.img

$(BUILD_DIR)/valkyrie.img: bootloader kernel
	@dd if=/dev/zero of=$(BUILD_DIR)/valkyrie.img bs=512 count=2880
	@mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/valkyrie.img
	@dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/valkyrie.img conv=notrunc
	@mcopy -i $(BUILD_DIR)/valkyrie.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	@mcopy -i $(BUILD_DIR)/valkyrie.img test.txt "::test.txt"

#
# Bootloader
#
bootloader: $(BUILD_DIR)/bootloader.bin

$(BUILD_DIR)/bootloader.bin: always
	@$(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin

#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
	@$(ASM) $(SRC_DIR)/kernel/main.asm -f bin -o $(BUILD_DIR)/kernel.bin

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
	@mkdir -p $(BUILD_DIR)/tools
	@$(CC) -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c

#
# Always
#
always:
	@mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	@rm -rf $(BUILD_DIR)

run:
	@qemu-system-i386 -fda $(BUILD_DIR)/valkyrie.img
