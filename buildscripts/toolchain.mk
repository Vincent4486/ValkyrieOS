toolchain: toolchain_binutils toolchain_gcc

BINUTILS_URL=https://ftp.gnu.org/gnu/binutils/binutils-2.45.tar.xz
GCC_URL=https://ftp.gnu.org/gnu/gcc/gcc-11.2.0/gcc-11.2.0.tar.xz
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
	cd toolchain && wget $(GCC_URL)
	cd toolchain && tar -xf gcc-11.2.0.tar.xz
	mkdir toolchain/gcc-build-11.2.0
	cd toolchain/gcc-build-11.2.0 && ../binutils-2.45/conf \
		--prefix="$(TOOLCHAIN_PREFIX)"	\
		--target=$TARGET				\
		--with-sysroot					\
		--disable-nls					\
		--endable-language=c,c++		\
		--without-headers				\

	$(MAKE) -j4 -C toolchain/gcc-build-11.2.0 all-gcc all-target-libgcc
	$(MAKE) -C toolchain/gcc-build-11.2.0 install-gcc install-target-libgcc
