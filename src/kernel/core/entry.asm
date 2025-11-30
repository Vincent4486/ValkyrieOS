; SPDX-License-Identifier: AGPL-3.0-or-later

[bits 32]

section .multiboot align=4
    ; Multiboot v1 header (magic, flags, checksum)
    ; magic = 0x1BADB002
    ; flags = 0x00000003  ; request memory info and boot_device (no addr fields)
    ; checksum = -(magic + flags)
    dd 0x1BADB002
    dd 0x00000003
    dd (-(0x1BADB002 + 0x00000003))

section .entry
; Entry point symbol - linker ENTRY points here
global _start
extern start

_start:
    ; GRUB passes: EAX = 0x2BADB002 (magic), EBX = pointer to multiboot_info
    cmp eax, 0x2BADB002
    jne .no_multiboot

    ; EBX -> multiboot_info structure
    mov esi, ebx
    ; boot_device field is at offset 0x0C in multiboot_info
    mov ebx, dword [esi + 0x0c]
    ; drive number is stored in the highest byte (per multiboot spec)
    shr ebx, 24
    ; Set up a small temporary stack to avoid relying on GRUB-provided stack
    mov esp, 0x90000
    mov ebp, esp

    ; Push args for C start(bootDrive, partitionPtr)
    push dword 0        ; partitionPtr (unused by main.c) = NULL
    push ebx            ; bootDrive (32-bit value; C will take low byte)
    call start
    jmp $

.no_multiboot:
    ; No multiboot info present.
    ; If the kernel was started by our stage2 bootloader it was called as:
    ;    kernel_entry(bootDrive, partitionPtr)
    ; In that case the bootDrive and partitionPtr are on the caller stack
    ; ([esp+4] and [esp+8]). We need to grab those, establish a fresh
    ; kernel stack, then call the C `start` function with the same args.
    ;
    ; Stack at entry: [esp] = return_addr, [esp+4] = bootDrive, [esp+8] = partitionPtr
    mov eax, [esp + 4]    ; bootDrive (32-bit, low byte used)
    mov edx, [esp + 8]    ; partitionPtr

    ; Set up a small temporary stack for the kernel (same as multiboot path)
    mov esp, 0x90000
    mov ebp, esp

    ; Push arguments for C start(partitionPtr, bootDrive)
    push edx            ; partitionPtr
    push eax            ; bootDrive
    call start
    jmp $
