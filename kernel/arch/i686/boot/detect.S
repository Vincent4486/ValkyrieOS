// SPDX-License-Identifier: AGPL-3.0-or-later

.global get_arch
get_arch:
    pushl %ebp
    movl %esp, %ebp
    
    # Get the pointer argument (at ebp+8)
    movl 8(%ebp), %eax    # eax = pointer to structure element
    
    # Write 1 to that location
    movl $1, (%eax)       # *pointer = 1
    
    # Clean up and return void
    movl %ebp, %esp
    popl %ebp
    ret

.globl get_cpu_count
get_cpu_count:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    
    movl 8(%ebp), %ecx      # ecx = pointer to uint32_t
    
    # Get CPUID features
    movl $1, %eax
    cpuid
    
    # Check for Hyper-Threading Technology
    testl $(1 << 28), %edx
    jz .single_core
    
    # Get logical processor count from ebx[23:16]
    movl %ebx, %eax
    shrl $16, %eax
    andl $0xFF, %eax
    
    # If 0, fall back to single core
    testl %eax, %eax
    jnz .store_count
    
.single_core:
    movl $1, %eax

.store_count:
    # Write to the pointer
    movl %eax, (%ecx)
    
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret

/*
 * Function: void get_cpu_brand(char *brand)
 * brand: pointer to 13-byte buffer (12 chars + null terminator)
 * Gets CPU vendor string via CPUID
 */
.globl get_cpu_brand
get_cpu_brand:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    
    movl 8(%ebp), %edi      # edi = pointer to brand buffer
    
    # CPUID leaf 0 - Get vendor string
    xorl %eax, %eax         # eax = 0
    cpuid                   # Get vendor ID
    
    # Store vendor string (12 bytes: EBX, EDX, ECX order)
    movl %ebx, (%edi)       # First 4 bytes
    movl %edx, 4(%edi)      # Next 4 bytes  
    movl %ecx, 8(%edi)      # Last 4 bytes
    
    # Null terminate
    movb $0, 12(%edi)
    
    popl %edi
    popl %esi
    popl %ebx
    movl %ebp, %esp
    popl %ebp
    ret