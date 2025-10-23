include scripts/config.mk

.PHONY: all floppy_image kernel jvm bootloader clean always run doc debug format

all: floppy_image

include scripts/toolchain.mk
# Documentation build target (runs the Makefile in docs/)
# Set DOC_TARGET to 'latexmk' or 'pdf' to change behavior. Default: pdf
DOC_TARGET ?= pdf
# Docs wrapper targets
doc:
	@echo "Building docs ($(DOC_TARGET))"
	$(MAKE) -C docs clean
	$(MAKE) -C docs $(DOC_TARGET)

#
# Floppy image
#
include scripts/image.mk

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
	@$(MAKE) -C src/kernel/core BUILD_DIR=$(abspath $(BUILD_DIR))

#
# JVM
#

jvm: $(BUILD_DIR)/jvm.bin

$(BUILD_DIR)/jvm.bin: always
	@$(MAKE) -C src/kernel/jvm BUILD_DIR=$(abspath $(BUILD_DIR))


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
	@rm -rf $(BUILD_DIR)/*

run:
	@qemu-system-i386 -fda build/valkyrie_os.img &> /dev/null

debug:
	@qemu-system-i386 -fda build/valkyrie_os.img -S -s

format:
	@clang-format -i $(C_FILES) $(HEADER_FILES)
