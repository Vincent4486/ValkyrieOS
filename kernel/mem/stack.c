// SPDX-License-Identifier: AGPL-3.0-or-later

#include "stack.h"
#include <mem/vmm.h>
#include <mem/heap.h>
#include <arch/i686/mem/stack.h>
#include <string.h>

/**
 * Generic stack management implementation
 * Platform-specific setup in arch/i686/mem/stack.c
 */

static Stack *kernel_stack = NULL;

/**
 * Initialize stack subsystem for the OS
 */
void Stack_Initialize(void) {
    Stack_InitializeKernel();
}

/**
 * Initialize kernel stack (delegates to architecture code)
 */
void Stack_InitializeKernel(void) {
    i686_Stack_InitializeKernel();
}

/**
 * Create a new user stack
 */
Stack *Stack_Create(size_t size) {
    if (size == 0) return NULL;
    
    // Allocate Stack structure
    Stack *stack = (Stack *)Heap_Alloc(sizeof(Stack));
    if (!stack) return NULL;
    
    // Allocate stack memory from kernel heap
    uint8_t *data = (uint8_t *)Heap_Alloc(size);
    if (!data) {
        Heap_Free(stack);
        return NULL;
    }
    
    // Initialize stack structure
    // Stack grows downward: base = highest address, current starts at base
    stack->base = (uint32_t)data + size;  // Top of allocated region
    stack->size = size;
    stack->current = stack->base;
    stack->data = data;
    
    return stack;
}

/**
 * Destroy a user stack
 */
void Stack_Destroy(Stack *stack) {
    if (!stack) return;
    
    // Free allocated data
    if (stack->data) {
        Heap_Free(stack->data);
    }
    
    // Free Stack structure
    Heap_Free(stack);
}

/**
 * Push data onto a stack (grows downward on x86)
 */
uint32_t Stack_Push(Stack *stack, const void *data, size_t size) {
    if (!stack || !data || size == 0) return 0;
    
    // Check for stack overflow
    if (!Stack_HasSpace(stack, size)) {
        return 0;  // Stack overflow
    }
    
    // Move stack pointer down (x86 stack grows downward)
    stack->current -= size;
    
    // Copy data to stack
    memcpy((void *)stack->current, data, size);
    
    return stack->current;
}

/**
 * Pop data from a stack
 */
uint32_t Stack_Pop(Stack *stack, void *data, size_t size) {
    if (!stack || !data || size == 0) return 0;
    
    // Check for stack underflow
    if (stack->current + size > stack->base) {
        return 0;  // Stack underflow
    }
    
    // Copy data from stack
    memcpy(data, (void *)stack->current, size);
    
    // Move stack pointer up
    stack->current += size;
    
    return stack->current;
}

/**
 * Set stack pointer with bounds checking
 */
int Stack_SetSP(Stack *stack, uint32_t sp) {
    if (!stack) return 0;
    
    uint32_t stack_bottom = (uint32_t)stack->data;
    uint32_t stack_top = stack->base;
    
    // Verify SP is within stack bounds
    if (sp >= stack_bottom && sp <= stack_top) {
        stack->current = sp;
        return 1;
    }
    
    return 0;
}

/**
 * Check if stack has enough free space
 */
int Stack_HasSpace(Stack *stack, size_t required) {
    if (!stack || required == 0) return 0;
    
    uint32_t stack_bottom = (uint32_t)stack->data;
    
    // Calculate free space from current SP to bottom
    if (stack->current <= stack_bottom) {
        return 0;  // Stack corrupted or at minimum
    }
    
    size_t free_space = stack->current - stack_bottom;
    return free_space >= required;
}

/**
 * Get kernel stack
 */
Stack *Stack_GetKernel(void) {
    return kernel_stack;
}

/**
 * Platform wrappers -> architecture-specific implementations
 */

void Stack_SetupProcess(Stack *stack, uint32_t entry_point) {
    i686_Stack_SetupProcess(stack, entry_point);
}

uint32_t Stack_GetESP(void) {
    return i686_Stack_GetESP();
}

uint32_t Stack_GetEBP(void) {
    return i686_Stack_GetEBP();
}

void Stack_SetRegisters(uint32_t esp, uint32_t ebp) {
    i686_Stack_SetRegisters(esp, ebp);
}

/**
 * Stack self-test (returns 1 on success, 0 on failure)
 */
int stack_self_test(void) {
    // Create/destroy
    Stack *s = Stack_Create(4096);
    if (!s) return 0;

    uint32_t sp0 = s->current;

    // Push/pop
    uint32_t val = 0xAABBCCDD;
    if (!Stack_Push(s, &val, sizeof(val))) { Stack_Destroy(s); return 0; }
    uint32_t popped = 0;
    if (!Stack_Pop(s, &popped, sizeof(popped))) { Stack_Destroy(s); return 0; }
    if (popped != val || s->current != sp0) { Stack_Destroy(s); return 0; }

    // Bounds check
    if (!Stack_HasSpace(s, 1024)) { Stack_Destroy(s); return 0; }
    if (Stack_SetSP(s, 0xFFFFFFFF)) { Stack_Destroy(s); return 0; }

    Stack_Destroy(s);
    return 1;
}
