# Valkyrie OS
The Valkyrie Operating System is an operaing system designed 
for the x86 32-bit architecture. The operating system is intended 
to run an operating system, and all executable files are Java 
Archives (jar).

## Java Virtual Machine
The JVM on this operating system contain most jvm features as IO,  
But this operating system is command line based, so features in 
Java Swing is not useable. 

## Install
To install Valkyrie OS, you can use the pre-built floppy image from the release page of this repository. Or you can use the make tool to build the operating system locally, to build this operating system, please follow these steps.

### Mac OS

On Mac OS, you would need to install <a href="https://brew.sh">Homebrew</a>, you can use this link to install brew, then follow the following steps to install needed tools.

```brew install make mtools mkfs.fat nasm i686-elf-binutils i686-elf-gcc qemu-system-i386```

Then goto the directory of the operating system and run

```make```

You can see the final disk image inside the ```build``` directory, then run

```qemu-system-i386 -fda build/valkyrie_os.img``` to run the OS inside a virtual machine.

### Linux
On linux, you can follow the steps for Mac OS, but swich Homebrew with your prefered package manager, some managers like ```apt``` does not have curtain packages.

### Windows
On Windows, please <a href="https://chocolatey.org/install">Chocolatey</a> to install packages.

## Development
Valkyrie OS is currently under development with the base system not finished, please wait untill the OS is released page to find a working OS.