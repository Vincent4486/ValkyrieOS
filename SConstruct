#!/usr/bin/env python3
import os
import subprocess

# Build configuration
BUILD_DIR = 'build'
SOURCE_DIR = '.'
MAKE_DISK_SIZE = 16777216  # 16MB

# Target architecture
TARGET = 'i686-elf'
TOOLCHAIN_DIR = os.path.abspath('./toolchain/i686-elf/bin')

# Add toolchain to PATH
env_path = os.environ.get('PATH', '')
os.environ['PATH'] = TOOLCHAIN_DIR + os.pathsep + env_path

# Create base environment for host tools
host_env = Environment(
    CC='gcc',
    CXX='g++',
    AS='nasm',
    ENV=os.environ,
)

# Create environment for target (i686-elf cross-compiler)
target_env = Environment(
    CC=f'{TARGET}-gcc',
    CXX=f'{TARGET}-g++',
    AS='nasm',
    LINK=f'{TARGET}-gcc',
    AR=f'{TARGET}-ar',
    RANLIB=f'{TARGET}-ranlib',
    ENV=os.environ,
)

# Configure target environment flags
target_env.Append(
    CFLAGS=['-std=c99', '-g', '-ffreestanding', '-nostdlib'],
    CXXFLAGS=['-std=c++17', '-g', '-ffreestanding', '-nostdlib'],
    ASFLAGS=['-f', 'elf'],
    LINKFLAGS=['-nostdlib'],
    LIBS=['gcc'],
)

# Export build directory and config
target_env['BUILD_DIR'] = BUILD_DIR
target_env['MAKE_DISK_SIZE'] = MAKE_DISK_SIZE
host_env['BUILD_DIR'] = BUILD_DIR

# Export build directory and config
target_env['BUILD_DIR'] = BUILD_DIR
target_env['MAKE_DISK_SIZE'] = MAKE_DISK_SIZE
host_env['BUILD_DIR'] = BUILD_DIR

# Export environments for SConscript files
Export('host_env', 'target_env', 'BUILD_DIR', 'TARGET', 'MAKE_DISK_SIZE')

# Build components using SConscript files
stage1_bin = SConscript('src/bootloader/stage1/SConscript', variant_dir=f'{BUILD_DIR}/stage1', duplicate=0)
stage2_bin = SConscript('src/bootloader/stage2/SConscript', variant_dir=f'{BUILD_DIR}/stage2', duplicate=0)
kernel_elf = SConscript('src/kernel/SConscript', variant_dir=f'{BUILD_DIR}/kernel', duplicate=0)

# Build floppy/disk images
floppy_img = target_env.Command(
    f'{BUILD_DIR}/valkyrie_os.img',
    [stage1_bin, stage2_bin, kernel_elf],
    f'BUILD_DIR={BUILD_DIR} ./scripts/make_floppy.sh $TARGET'
)
Depends(floppy_img, [stage1_bin, stage2_bin, kernel_elf])

disk_img = target_env.Command(
    f'{BUILD_DIR}/valkyrie_os.raw',
    [stage1_bin, stage2_bin, kernel_elf],
    f'BUILD_DIR={BUILD_DIR} sudo ./scripts/make_disk.sh $TARGET {MAKE_DISK_SIZE}'
)
Depends(disk_img, [stage1_bin, stage2_bin, kernel_elf])

# Aliases
Alias('bootloader', [stage1_bin, stage2_bin])
Alias('kernel', kernel_elf)
Alias('floppy_image', floppy_img)
Alias('disk_image', disk_img)
Alias('all', floppy_img)

# Default target
Default('floppy_image')

# Clean
Clean([stage1_bin, stage2_bin, kernel_elf, floppy_img, disk_img], BUILD_DIR)
