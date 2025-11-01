# Valkyrie OS
The Valkyrie Operating System is an operating system designed 
for the x86 32-bit architecture. The operating system is intended 
to run Java applications, and all executable files are Java 
Archives (jar).

## Java Virtual Machine
The JVM on this operating system contains most JVM features such as IO.  
However, this operating system is command line based, so features in 
Java Swing are not usable. 

## Install
To install Valkyrie OS, you can use the pre-built floppy image from the release page of this repository. Alternatively, you can use the build tools to compile the operating system locally. To build this operating system, please follow these steps.

### Mac OS

On Mac OS, you would need to install <a href="https://brew.sh">Homebrew</a>, you can use this link to install brew, then follow the following steps to install needed tools.

```brew install make mtools mkfs.fat nasm i686-elf-binutils i686-elf-gcc qemu-system-i386 perl```

Then go to the directory of the operating system and run

```make```

You can see the final disk image inside the ```build``` directory, then run

```qemu-system-i386 -fda build/valkyrie_os.img``` 

to run the OS inside a virtual machine.

### Linux
On Linux, you can follow the steps for Mac OS, but switch Homebrew with your preferred package manager. Some managers like ```apt``` do not have certain packages.

### Windows
On Windows, please use <a href="https://chocolatey.org/install">Chocolatey</a> to install packages.

## Development
Valkyrie OS is currently under development with the base system not yet complete. Please wait until the OS is released to find a working version.