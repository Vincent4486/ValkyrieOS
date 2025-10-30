# SCons Build System for ValkyrieOS

## Installation

First, install SCons:
```bash
# macOS
brew install scons

# or using pip
pip install scons
```

## Usage

### Build Commands

```bash
# Build floppy image (default target)
scons

# Build specific targets
scons bootloader      # Build stage1 and stage2 bootloader
scons kernel          # Build kernel only
scons floppy_image    # Build floppy disk image
scons disk_image      # Build hard disk image

# Build everything
scons all

# Verbose output
scons -Q

# Parallel build (use 4 cores)
scons -j4

# Clean build directory
scons -c
```

## How It Works

The SConstruct file:

1. **Sets up environments**:
   - `host_env`: For host tools (gcc, nasm)
   - `target_env`: For cross-compiler (i686-elf-gcc)
   - `stage1_env`: For 16-bit bootloader
   - `kernel_env`: For kernel with specific flags

2. **Builds components**:
   - Stage 1: `build/stage1.bin` (16-bit bootloader)
   - Stage 2: `build/stage2.bin` (32-bit bootloader)
   - Kernel: `build/kernel.elf` (i686 kernel)

3. **Creates disk images**:
   - Floppy: `build/valkyrie_os.img` (1.44MB)
   - Disk: `build/valkyrie_os.raw` (configurable size)

## Advantages over Make

- Automatic dependency tracking (no manual header dependencies)
- Built-in parallel builds
- Cleaner syntax
- Better cross-platform support
- Automatic directory creation

## Differences from Makefile

- No need for `make clean` before rebuilds - SCons tracks dependencies automatically
- Parallel builds with `-j` flag work more reliably
- No need for `.PHONY` targets
- Better handling of generated files (like ISR generation)

## Toolchain Setup

The SConstruct assumes your i686-elf toolchain is in:
```
./toolchain/i686-elf/bin/
```

If your toolchain is elsewhere, edit the `TOOLCHAIN_DIR` variable in SConstruct.

## Testing

To test the SCons build:

```bash
# Clean old builds
rm -rf build/

# Build with SCons
scons

# Run in QEMU
qemu-system-i386 -fda build/valkyrie_os.img
```

## Troubleshooting

**Problem**: `Command not found: i686-elf-gcc`
**Solution**: Make sure toolchain is built and TOOLCHAIN_DIR is correct

**Problem**: Permission denied on disk_image
**Solution**: The disk_image target requires sudo (same as Makefile)

**Problem**: ISR generation fails
**Solution**: Make sure scripts/generate_isr.sh is executable:
```bash
chmod +x scripts/generate_isr.sh
```
