# image.mk - create floppy image and copy files into it
.PHONY: floppy_image
IMAGE := $(BUILD_DIR)/valkyrie_os.img

floppy_image: $(BUILD_DIR)/valkyrie_os.img

$(BUILD_DIR)/valkyrie_os.img: bootloader kernel
	@dd if=/dev/zero of=$@ bs=512 count=2880 >/dev/null
	@mkfs.fat -F 12 -n "VALKYRIE" $@ >/dev/null
	@dd if=$(BUILD_DIR)/stage1.bin of=$@ conv=notrunc >/dev/null
	@mmd -i $@ "::sys"
	@mcopy -i $@ $(BUILD_DIR)/stage2.bin "::stage2.bin"
	@mcopy -i $@ $(BUILD_DIR)/kernel.bin "::sys/kernel.bin"
	@mcopy -i $@ test.txt "::test.txt"
	@mmd -i $@ "::mydir"
	@mcopy -i $@ test.txt "::mydir/test.txt"
	@echo "--> Created: " $@