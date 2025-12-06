// SPDX-License-Identifier: AGPL-3.0-or-later

#include <stdint.h>

typedef struct {
    uint32_t pid;                    // Process ID
    uint32_t ppid;                   // Parent process ID
    uint32_t state;                  // RUNNING, READY, BLOCKED, TERMINATED
    
    // Memory management
    void* page_directory;            // Points to process's page directory
    uint32_t heap_start;             // Start of heap segment
    uint32_t heap_end;               // Current heap end
    uint32_t stack_start;            // Start of user stack
    uint32_t stack_end;              // End of user stack
    
    // CPU state
    uint32_t eip;                    // Instruction pointer
    uint32_t esp;                    // Stack pointer
    uint32_t ebp;                    // Base pointer
    uint32_t eax, ebx, ecx, edx;     // General purpose registers
    uint32_t esi, edi;               // Index registers
    uint32_t eflags;                 // Flags register
    
    // File descriptors
    void* fd_table[16];              // Open file descriptors (per-process)
    
    // Scheduling
    uint32_t priority;               // Priority level
    uint32_t ticks_remaining;        // Time slice remaining
    
    // Signals
    uint32_t signal_mask;            // Blocked signals
    
    // Exit status
    int exit_code;                   // Exit status when terminated
} Process;