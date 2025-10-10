toolchain: toolchain_binutils toolchain_gcc

BINUTILS_URL=https://ftp.gnu.org/gnu/binutils/binutils-2.45.tar.xz
TOOLCHAIN_PREFIX=toolchain/i686-elf
TARGET=i686-elf
MAKE=make

toolchain_binutils:
	mkdir toolchain 
	cd toolchain && wget $(BINUTILS_URL)
	cd toolchain && tar -xf binutils-2.45.tar.xz
	mkdir toolchain/binutils-build-2.45
	cd toolchain/binutils-build-2.45 && ../binutils-2.45/conf \
		--prefix="$(TOOLCHAIN_PREFIX)"	\
		--target=$TARGET				\
		--with-sysroot					\
		--disable-nls					\
		--deiable-werror
	$(MAKE) -j4 -C toolchain/binutils-build-2.45
	$(MAKE) -C toolchain/binutils-build-2.45 install

toolchain_gcc: toolchain_binutils
	mkdir toolchain 
	cd toolchain && wget $(BINUTILS_URL)
	cd toolchain && tar -xf binutils-2.45.tar.xz
	mkdir toolchain/binutils-build-2.45
	cd toolchain/binutils-build-2.45 && ../binutils-2.45/conf \
		--prefix="$(TOOLCHAIN_PREFIX)"	\
		--target=$TARGET				\
		--with-sysroot					\
		--disable-nls					\
		--deiable-werror
	$(MAKE) -j4 -C toolchain/binutils-build-2.45
	$(MAKE) -C toolchain/binutils-build-2.45 install
