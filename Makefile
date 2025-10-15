ASM=nasm
CC=gcc
CC16=wcc
LD16=wlink

SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy_image kernel bootloader clean always tools_fat run

all: clean floppy_image

#
# Floppy image
#
floppy_image: $(BUILD_DIR)/valkyrie.img

$(BUILD_DIR)/valkyrie.img: bootloader kernel
	dd if=/dev/zero of=$(BUILD_DIR)/valkyrie.img bs=512 count=2880
	mkfs.fat -F 12 -n "VALKYRIE" $(BUILD_DIR)/valkyrie.img
	dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/valkyrie.img conv=notrunc
	mcopy -i $(BUILD_DIR)/valkyrie.img $(BUILD_DIR)/stage2.bin "::STAGE2.BIN"
	mcopy -i $(BUILD_DIR)/valkyrie.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	mcopy -i $(BUILD_DIR)/valkyrie.img test.txt "::test.txt"
	mmd -i $(BUILD_DIR)/valkyrie.img "::mydir"
	mcopy -i $(BUILD_DIR)/valkyrie.img test2.txt "::mydir/test2.txt"

#
# Bootloader
#
bootloader: stage1 stage2

stage1: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: always
	$(MAKE) -C $(SRC_DIR)/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR))

stage2: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: always
	$(MAKE) -C $(SRC_DIR)/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
	$(MAKE) -C $(SRC_DIR)/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
	mkdir -p $(BUILD_DIR)/tools
	$(MAKE) -C tools/fat BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	$(MAKE) -C $(SRC_DIR)/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	$(MAKE) -C $(SRC_DIR)/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	$(MAKE) -C $(SRC_DIR)/kernel BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	rm -rf $(BUILD_DIR)/*

run:
	qemu-system-i386 -fda build/valkyrie.img

debug:
	qemu-system-i386 -fda build/valkyrie.img -S -s
