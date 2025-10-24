TARGET_ASMFLAGS += -f elf
TARGET_CFLAGS += -ffreestanding -nostdlib -I.
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

TARGET_CXXFLAGS += -ffreestanding -nostdlib -I.


HEADERS_C=$(shell find . -type f -name "*.h")
HEADERS_ASM=$(shell find . -type f -name "*.inc")
HEADERS_CPP=$(shell find . -type f -name "*.hpp" -o -name "*.h")

SOURCES_C=$(shell find . -type f -name "*.c")
SOURCES_ASM=$(shell find . -type f -name "*.asm")
SOURCES_CPP=$(shell find . -type f -name "*.cpp")

OBJECTS_C=$(patsubst %.c, $(BUILD_DIR)/kernel/c/%.obj, $(SOURCES_C))
OBJECTS_ASM=$(patsubst %.asm, $(BUILD_DIR)/kernel/asm/%.obj, $(SOURCES_ASM))
OBJECTS_CPP=$(patsubst %.cpp, $(BUILD_DIR)/kernel/cpp/%.obj, $(SOURCES_CPP))

.PHONY: all kernel clean always

all: kernel

kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(OBJECTS_ASM) $(OBJECTS_C) $(OBJECTS_CPP)
	@$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/kernel.map -o $@ $^ $(TARGET_LIBS)
	@echo "--> Created:  kernel.bin"

$(BUILD_DIR)/kernel/c/%.obj: %.c $(HEADERS_C)
	@mkdir -p $(@D)
	@$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<
	@echo "--> Compiled: " $<

$(BUILD_DIR)/kernel/asm/%.obj: %.asm $(HEADERS_ASM)
	@mkdir -p $(@D)
	@$(TARGET_ASM) $(TARGET_ASMFLAGS) -o $@ $<
	@echo "--> Compiled: " $<

$(BUILD_DIR)/kernel/cpp/%.obj: %.cpp $(HEADERS_CPP)
	@mkdir -p $(@D)
	@$(TARGET_CXX) $(TARGET_CXXFLAGS) -c -o $@ $<
	@echo "--> Compiled: " $<

arch/i686/isrs_gen.inc arch/i686/isrs_gen.c:
	@$(SOURCE_DIR)/scripts/generate_isr.sh $@

clean:
	rm -f $(BUILD_DIR)/kernel.bin