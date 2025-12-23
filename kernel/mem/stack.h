// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <stddef.h>
#include <cpu/process.h>

/**
 * Stack Management for ValkyrieOS
 * 
 * Manages kernel and per-process user stacks.
 * Platform-specific implementations in arch/i686/mem/stack.h|c
 */

/**
 * Stack information for a process or kernel context
 */
typedef struct {
    uint32_t base;      // Base address (top of stack, high address on x86)
    uint32_t size;      // Stack size in bytes
    uint32_t current;   // Current stack pointer
    uint8_t *data;      // Allocated stack memory
} Stack;

/**
 * Initialize stack subsystem for the OS
 * 
 * Sets up kernel stack infrastructure.
 * Called during kernel initialization (before creating processes).
 */
void Stack_Initialize(void);

/**
 * Initialize kernel stack
 * 
 * Sets up the kernel stack at a fixed location in kernel memory.
 * Called during kernel initialization.
 */
void Stack_InitializeKernel(void);

/**
 * Create a new user stack for a process
 * 
 * @param size Stack size in bytes (typically 64KB for user processes)
 * @return Pointer to new Stack structure, or NULL on failure
 * 
 * Allocates memory for the stack and initializes the Stack structure.
 * The stack grows downward (standard x86 behavior).
 */
Stack *Stack_Create(size_t size);

/**
 * Initialize a process's user stack
 * 
 * @param proc Process to initialize stack for
 * @param stack_top_va Virtual address of the top of the stack (e.g. 0xBFFFF000)
 * @param size Size of the stack in bytes
 * @return 0 on success, -1 on failure
 */
int Stack_ProcessInitialize(Process *proc, uint32_t stack_top_va, size_t size);

/**
 * Destroy a user stack
 * 
 * @param stack Stack to destroy
 * 
 * Frees the allocated stack memory and the Stack structure.
 */
void Stack_Destroy(Stack *stack);

/**
 * Push data onto a stack
 * 
 * @param stack Stack to push to
 * @param data Data to push
 * @param size Size of data in bytes
 * @return New stack pointer, or 0 on failure (stack overflow)
 */
uint32_t Stack_Push(Stack *stack, const void *data, size_t size);

/**
 * Pop data from a stack
 * 
 * @param stack Stack to pop from
 * @param data Buffer to receive popped data
 * @param size Size of data to pop in bytes
 * @return New stack pointer, or 0 on failure (stack underflow)
 */
uint32_t Stack_Pop(Stack *stack, void *data, size_t size);

/**
 * Get current stack pointer for a stack
 * 
 * @param stack Stack structure
 * @return Current stack pointer value
 */
static inline uint32_t Stack_GetSP(Stack *stack) {
    return stack ? stack->current : 0;
}

/**
 * Set current stack pointer
 * 
 * @param stack Stack structure
 * @param sp New stack pointer value
 * @return 1 on success, 0 if SP is out of valid range
 */
int Stack_SetSP(Stack *stack, uint32_t sp);

/**
 * Check if stack has enough free space
 * 
 * @param stack Stack structure
 * @param required Required free space in bytes
 * @return 1 if enough space, 0 otherwise
 */
int Stack_HasSpace(Stack *stack, size_t required);

/**
 * Platform-specific: Setup stack for process execution
 * 
 * Called after ELF loading to prepare stack for:
 * - argc/argv
 * - Environment variables
 * - Return address
 */
void Stack_SetupProcess(Stack *stack, uint32_t entry_point);

/**
 * Platform-specific: Get current ESP register
 */
uint32_t Stack_GetESP(void);

/**
 * Platform-specific: Get current EBP register
 */
uint32_t Stack_GetEBP(void);

/**
 * Platform-specific: Set ESP and EBP registers
 */
void Stack_SetRegisters(uint32_t esp, uint32_t ebp);

/**
 * Get kernel stack
 * 
 * @return Pointer to kernel stack structure
 */
Stack *Stack_GetKernel(void);

/**
 * Stack subsystem self-test
 *
 * @return 1 on success, 0 on failure
 */
int stack_self_test(void);

#endif