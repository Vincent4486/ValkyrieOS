# ValkyrieOS glibc Implementation Roadmap

## Phase 1: Core OS Infrastructure (Current)

### Memory Management
- [ ] Virtual memory / paging (per-process address spaces)
- [ ] Heap management (brk/sbrk syscalls for malloc)
- [ ] Stack management (per-process user stacks)
- [ ] Memory protection (read/write/execute permissions)
- [ ] Page tables and MMU setup

### Process Management
- [x] Process Control Block (PCB) structure
- [ ] Process creation (fork/execve syscalls)
- [ ] Process scheduling & context switching
- [ ] Process termination (exit syscall)
- [ ] Signal handling (signal syscalls)
- [ ] User-mode ring 3 execution
- [ ] User/kernel mode switching

### Filesystem
- [x] FAT filesystem read/write (basic done)
- [x] `FAT_Create()` - create new files
- [x] `FAT_Delete()` - delete files
- [x] Multi-cluster file support (files > 1 cluster)
- [ ] Device files abstraction
- [ ] File descriptors table per process
- [ ] `FAT_Stat()` - file metadata

### Terminal Support
- [ ] TTY driver for `/dev/tty0`
- [ ] Terminal line discipline (buffering, echo control)
- [ ] ANSI escape sequence support
- [ ] Keyboard input handling
- [ ] Terminal control ioctl calls

## Phase 2: Essential Syscalls

### File I/O Syscalls
- [ ] `read` - read from file descriptor
- [ ] `write` - write to file descriptor
- [ ] `open` - open file
- [ ] `close` - close file descriptor
- [ ] `lseek` - seek in file
- [ ] `fstat`, `stat`, `lstat` - file metadata
- [ ] `ioctl` - device control
- [ ] `fcntl` - file control
- [ ] `access` - check permissions

### Memory Syscalls
- [ ] `brk` - set data segment size (for malloc)
- [ ] `mmap`, `munmap` - memory mapping
- [ ] `mprotect` - set memory protection
- [ ] `mremap` - remap memory region

### Process Syscalls
- [ ] `exit`, `exit_group` - process termination
- [ ] `fork` - process creation
- [ ] `clone` - thread/process creation
- [ ] `execve` - execute program
- [ ] `wait4`, `waitpid` - wait for child process
- [ ] `getpid`, `getppid` - process IDs
- [ ] `getuid`, `setuid` - user IDs
- [ ] `geteuid`, `getgid`, `getegid` - group IDs

### Signal Syscalls
- [ ] `signal`, `sigaction` - signal handlers
- [ ] `sigprocmask`, `sigpending` - signal masking
- [ ] `kill` - send signal
- [ ] `pause` - wait for signal
- [ ] `alarm` - timer signal
- [ ] `sigreturn` - return from signal handler

### Time Syscalls
- [ ] `time` - current time
- [ ] `gettimeofday`, `clock_gettime` - precise time
- [ ] `nanosleep` - sleep
- [ ] `select`, `poll` - I/O multiplexing

### Miscellaneous Syscalls
- [ ] `uname` - system info
- [ ] `getpagesize` - page size
- [ ] `prctl` - process control
- [ ] `umask` - file creation mask
- [ ] `nice`, `getpriority`, `setpriority` - scheduling

## Phase 3: Functionality Support

### Core Functionality (glibc will syscall for these)
- [ ] Dynamic memory management - heap allocation/deallocation
- [ ] Formatted I/O - output to file descriptors, input from files
- [ ] File streaming - buffered read/write with file streams
- [ ] I/O multiplexing - select/poll on multiple file descriptors
- [ ] String/memory manipulation - copy, search, compare operations
- [ ] Time/clock queries - current time, timers
- [ ] Locale support - character encoding, sorting

### Provided Headers
- [x] `<stdio.h>` - basic version
- [ ] `<stdlib.h>` - allocation, exit utilities
- [x] `<string.h>` - basic version
- [ ] `<time.h>` - time/clock functions
- [ ] `<sys/stat.h>` - file metadata queries
- [ ] `<fcntl.h>` - file opening flags
- [ ] `<unistd.h>` - POSIX utilities
- [ ] `<signal.h>` - signal handling
- [ ] `<errno.h>` - error reporting
- [ ] `<limits.h>` - system limits
- [ ] `<stddef.h>` - standard type definitions

### Threading Support (glibc will handle, you provide primitives)
- [ ] Thread creation/joining syscalls
- [ ] Mutex primitives (futex syscall)
- [ ] Condition variable support
- [ ] Thread-local storage (TLS)
- [ ] Atomic operations

## Phase 4: Advanced Features

### Device Support
- [ ] `/dev/null` device file
- [ ] `/dev/zero` device file
- [ ] `/dev/random`, `/dev/urandom` - entropy
- [ ] Block device interface
- [ ] Character device interface
- [ ] Device node mounting

### Filesystem Extended
- [ ] Permission checking (uid/gid/mode)
- [ ] Directory creation (`mkdir`)
- [ ] Directory deletion (`rmdir`)
- [ ] Symlink support
- [ ] Hard link support
- [ ] File ownership and chmod

### Dynamic Linking
- [x] Custom dylib system (exists)
- [x] `/lib`, `/usr/lib` layouts (done)
- [ ] LD_LIBRARY_PATH support
- [ ] Symbol versioning
- [ ] RPATH support

### Networking
- [ ] Socket syscalls (`socket`, `bind`, `listen`, `connect`, `accept`)
- [ ] UDP/TCP support
- [ ] DNS resolution
- [ ] Network device drivers
