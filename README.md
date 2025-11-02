# Valkyrie OS
The Valkyrie Operating System is an operaing system designed 
for the x86 32-bit architecture, OS versions in the future are planed to support x64. The operating system is where all executable files are Java 
Archives ```jar``` or Java Bytecode or ```.class``` files. 

## Java Virtual Machine
The JVM on this operating system contain most jvm features as IO,  
But this operating system is command line based, so features in 
Java Swing is not useable. 

## Install
To install Valkyrie OS, you can use the pre-built floppy images or hard disk images from the release page of this repository. Or you can use the make tool to build the operating system locally, to build this operating system, please follow these steps, building the Valkyrie OS requires the following dependencies:

### Allways needed:
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
Linux is the only host operating system supported for building the Valkyrie OS, since building requires disk tools like ```parted``` to build.

### Optional
```
perl
clang-format
LaTeX
make
```
These tools are for extra tools as autorun for development and build documentation.

## Development
Valkyrie OS is currently under development with the base system not finished, please wait untill the OS is released page to find a full working OS.