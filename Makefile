include scripts/config.mk

.PHONY: all floppy_image disk_image kernel bootloader clean always

all: floppy_image

include scripts/toolchain.mk

#
# Floppy image
#
floppy_image: $(BUILD_DIR)/valkyrie_os.img

$(BUILD_DIR)/valkyrie_os.img: bootloader kernel
	./scripts/make_floppy.sh $@
#echo "--> Created: " $@


#
# Disk image
#
disk_image: $(BUILD_DIR)/valkyrie_os.raw

$(BUILD_DIR)/valkyrie_os.raw: bootloader kernel
	sudo ./scripts/make_disk.sh $@ $(MAKE_DISK_SIZE)
#echo "--> Created: " $@

#
# Bootloader
#
bootloader: stage1 stage2

stage1: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: always
	$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR))

stage2: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: always
	$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
	$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always tools/fat/fat.c
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
	$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	rm -rf $(BUILD_DIR)/*
#echo "--> Cleaned build directory..."
