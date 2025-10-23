TARGET_CFLAGS += -ffreestanding -nostdlib -I. -I../core
TARGET_LIBS += -lgcc
TARGET_LINKFLAGS += -T linker.ld -nostdlib

HEADERS_CPP=$(shell find . -type f -name "*.h")
SOURCES_CPP=$(shell find . -type f -name "*.cpp")
OBJECTS_CPP=$(patsubst %.cpp, $(BUILD_DIR)/kernel/jvm/cpp/%.obj, $(SOURCES_CPP))

.PHONY: all jvm clean always

all: jvm

jvm: $(BUILD_DIR)/jvm.bin

$(BUILD_DIR)/jvm.bin: $(OBJECTS_CPP)
	@$(TARGET_LD) $(TARGET_LINKFLAGS) -Wl,-Map=$(BUILD_DIR)/kernel.map -o $@ $^ $(TARGET_LIBS)
	@echo "--> Created:  jvm.bin"

$(BUILD_DIR)/kernel/jvm/cpp/%.obj: %.c $(HEADERS_CPP)
	@mkdir -p $(@D)
	@$(TARGET_CC) $(TARGET_CFLAGS) -c -o $@ $<
	@echo "--> Compiled: " $<

clean: 
	@rm $(BUILD_DIR)/jvm.bin