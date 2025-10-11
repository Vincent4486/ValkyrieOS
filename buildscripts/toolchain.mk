TOOLCHAIN_PREFIX=$(abspath toolchain/$(TARGET))
export PATH := $(TOOLCHAIN_PREFIX)/bin:$(PATH)
export TARGET_CC=$(TARGET)-gcc
export TARGET_CXX=$(TARGET)-g++
export TARGET_LD=$(TARGET)-ld

toolchain: toolchain_binutils toolchain_gcc

MAKE=make

BINUTILS_BUILD_DIR=toolchain/binutils-build-$(BINUTILS_VERSION)
GCC_BUILD_DIR=toolchain/gcc-build-$(GCC_VERSION)

toolchain_binutils:
	mkdir toolchain 
	cd toolchain && wget $(BINUTILS_URL)
	cd toolchain && tar -xf binutils-$(BINUTILS_VERSION).tar.xz
	mkdir $(BINUTILS_BUILD_DIR)
	cd $(BINUTILS_BUILD_DIR) && ../binutils-$(BINUTILS_VERSION)/configure \
		--prefix="$(TOOLCHAIN_PREFIX)"	\
		--target=$(TARGET)				\
		--with-sysroot					\
		--disable-nls					\
		--deiable-werror
	$(MAKE) -j4 -C $(BINUTILS_BUILD_DIR)
	$(MAKE) -C $(BINUTILS_BUILD_DIR) install

toolchain_gcc: toolchain_binutils
	cd toolchain && wget $(GCC_URL)
	cd toolchain && tar -xf gcc-$(GCC_VERSION).tar.xz
	mkdir $(GCC_BUILD_DIR)
	cd $(GCC_BUILD_DIR) && ../gcc-$(GCC_VERSION)/configure \
		--prefix="$(TOOLCHAIN_PREFIX)"	\
		--target=$(TARGET)				\
		--with-sysroot					\
		--disable-nls					\
		--endable-language=c,c++		\
		--without-headers				\

	$(MAKE) -j4 -C $(GCC_BUILD_DIR) all-gcc all-target-libgcc
	$(MAKE) -C $(GCC_BUILD_DIR) install-gcc install-target-libgcc
