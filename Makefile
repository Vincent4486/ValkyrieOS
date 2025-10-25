include scripts/config.mk

<<<<<<< HEAD
.PHONY: all floppy_image disk_image kernel bootloader clean always
all: floppy_image
=======
.PHONY: all floppy_image disk_image kernel bootloader clean always tools_fat

all: floppy_image tools_fat
>>>>>>> c8852e10b58ca765b20692c12c5aaafede882141

include scripts/toolchain.mk

#
# Floppy image
#
floppy_image: $(BUILD_DIR)/main_floppy.img

<<<<<<< HEAD
$(BUILD_DIR)/valkyrie_os.img: bootloader kernel
	@./scripts/make_floppy.sh $@
	@echo "--> Created: " $@


#
# Disk image
#
disk_image: $(BUILD_DIR)/valkyrie_os.raw

$(BUILD_DIR)/main_disk.raw: bootloader kernel
	@./scripts/make_disk.sh $@ $(MAKE_DISK_SIZE)
=======
$(BUILD_DIR)/main_floppy.img: bootloader kernel
	@./scripts/make_floppy_image.sh $@
>>>>>>> c8852e10b58ca765b20692c12c5aaafede882141
	@echo "--> Created: " $@


#
# Disk image
#
disk_image: $(BUILD_DIR)/main_disk.raw

$(BUILD_DIR)/main_disk.raw: bootloader kernel
	@./scripts/make_disk_image.sh $@ $(MAKE_DISK_SIZE)
	@echo "--> Created: " $@


#
# Bootloader
#
bootloader: stage1 stage2

stage1: $(BUILD_DIR)/stage1.bin

$(BUILD_DIR)/stage1.bin: always
	@$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR))

stage2: $(BUILD_DIR)/stage2.bin

$(BUILD_DIR)/stage2.bin: always
	@$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always
	@$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always tools/fat/fat.c
	@mkdir -p $(BUILD_DIR)/tools
	@$(MAKE) -C tools/fat BUILD_DIR=$(abspath $(BUILD_DIR))

#
# Always
#
always:
	@mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	@$(MAKE) -C src/bootloader/stage1 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	@$(MAKE) -C src/bootloader/stage2 BUILD_DIR=$(abspath $(BUILD_DIR)) clean
	@$(MAKE) -C src/kernel BUILD_DIR=$(abspath $(BUILD_DIR)) clean
<<<<<<< HEAD
	@rm -rf $(BUILD_DIR)/*
	@echo "--> Build directory cleaned."

run:
	@echo "--> Running OS with QEMU..."
	@qemu-system-i386 -fda build/valkyrie_os.img &> /dev/null

debug-gdb:
	@echo "--> Debuging OS with GDB..."
	@qemu-system-i386 -fda build/valkyrie_os.img -S -s

debug-bochs:
	@echo "--> Debuging OS with Bochs..."
	@bochs -f .bochsrc

format:
	@for f in $(CPP_FILES) $(C_FILES) $(HEADER_FILES); do \
		if [ -f "$$f" ]; then \
			echo "--> Formatting: $$f"; \
			clang-format -i "$$f"; \
		fi; \
	done
=======
	@rm -rf $(BUILD_DIR)/*
>>>>>>> c8852e10b58ca765b20692c12c5aaafede882141
