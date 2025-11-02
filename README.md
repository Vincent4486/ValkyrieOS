# Valkyrie OS
The Valkyrie Operating System is an operating system designed 
for the x86 32-bit architecture. OS versions in the future are planned to support x64. The operating system is where all executable files are Java 
Archives ```jar``` or Java Bytecode or ```.class``` files. 

## Java Virtual Machine
The JVM on this operating system contains most JVM features such as IO. However, this operating system is command-line based, so features in Java Swing are not usable. 

## Install
To install Valkyrie OS, you can use the pre-built floppy images or hard disk images from the release page of this repository. Alternatively, you can use the build tool to build the operating system locally. To build this operating system, please follow these steps. Building the Valkyrie OS requires the following dependencies:

### Prerequisites

Before building Valkyrie OS, ensure you have the following installed:

#### Always needed:
```
bison
flex
mpfr
gmp
mpc
nasm
mtools
gcc
qemu
python3
python3-scons
python3-sh
python3-pyparted
```

Linux is the only host operating system supported for building the Valkyrie OS, since building requires disk tools like ```parted``` to work.

#### Optional
```
perl
clang-format
LaTeX
make
```
These tools are for extra utilities such as autorun for development and build documentation.

### Building from Source

1. **Install dependencies** (Ubuntu/Debian):
   ```bash
   sudo apt-get update
   sudo apt-get install -y bison flex libmpfr-dev libgmp-dev libmpc-dev nasm mtools gcc qemu python3 python3-scons python3-sh python3-pyparted
   ```

2. **Build the cross-toolchain** (this may take a while):
   ```bash
   ./scripts/base/toolchain.sh toolchain
   ```

3. **Build the operating system**:
   ```bash
   scons
   ```

   To customize the build, you can use various options:
   - `config=debug` or `config=release` - Build configuration (default: debug)
   - `arch=i686` - Target architecture (currently only i686 supported)
   - `imageType=floppy` or `imageType=disk` - Output image type (default: disk)
   - `imageFS=fat12|fat16|fat32|ext2` - Filesystem type (default: fat32)
   - `imageSize=250m` - Image size with suffixes k/m/g (default: 250m)
   - `outputFile=name` - Output filename without extension (default: valkyrie_os)

   Example:
   ```bash
   scons config=release imageType=floppy outputFile=valkyrie_floppy
   ```

4. **Run the image** with QEMU:
   ```bash
   scons run
   ```

   Or manually:
   ```bash
   ./scripts/base/qemu.sh disk build/i686_debug/valkyrie_os.img
   ```

### Debugging

To debug the operating system with GDB:
```bash
scons debug
```

Or manually:
```bash
./scripts/base/gdb.sh disk build/i686_debug/valkyrie_os.img
```

## Development
Valkyrie OS is currently under development with the base system not finished. Please wait until the OS is released to find a fully working OS.

### Project Structure

```
ValkyrieOS/
├── src/
│   ├── bootloader/          # Stage 1 and Stage 2 bootloaders
│   │   ├── stage1/          # Boot sector code
│   │   └── stage2/          # Extended bootloader
│   └── kernel/              # Kernel implementation
│       ├── arch/            # Architecture-specific code (i686)
│       ├── display/         # Display and keyboard drivers
│       ├── drivers/         # Device drivers (ATA, FDC, etc.)
│       ├── fs/              # Filesystem implementations (FAT12, FAT32)
│       ├── hal/             # Hardware abstraction layer
│       ├── jvm/             # Java Virtual Machine
│       ├── memory/          # Memory management
│       └── std/             # Standard library functions
├── image/                   # Disk image building scripts
├── scripts/
│   ├── base/                # Base utilities (qemu, gdb, toolchain)
│   └── scons/               # SCons build system utilities
└── SConstruct               # Main build configuration
```

### Architecture

- **Bootloader (Stage 1)**: Loads Stage 2 from disk
- **Bootloader (Stage 2)**: Initializes hardware and loads the kernel
- **Kernel**: Manages system resources, drivers, and JVM execution
- **JVM**: Executes Java bytecode programs

### Contributing

To contribute to Valkyrie OS:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes
4. Test your changes by building and running
5. Commit your changes (`git commit -am 'Add my feature'`)
6. Push to the branch (`git push origin feature/my-feature`)
7. Create a Pull Request

### License

See the LICENSE file for details on the project's license.

### Support

For issues, questions, or suggestions, please open an issue on the GitHub repository.