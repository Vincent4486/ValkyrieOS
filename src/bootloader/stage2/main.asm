bits 16

section .text

global entry

entry:
    cli
    
    ; Set VGA text mode first
    mov ax, 0x0003
    int 0x10
    
    ; Show progress
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x0753   ; 'S' for Start

    ; Setup stack
    mov ax, ds
    mov ss, ax
    mov sp, 0x1000
    mov bp, sp
    mov word [es:2], 0x0742   ; 'B' 

    ; Load GDT
    lgdt [gdt_descriptor]
    mov word [es:4], 0x0741   ; 'A'

    ; Switch to protected mode
    mov word [es:6], 0x0750   ; 'P' 
    mov eax, cr0
    or al, 1
    mov cr0, eax
    mov word [es:8], 0x074D   ; 'M'

    ; Show we're about to far jump
    mov word [es:10], 0x074A  ; 'J' for Jump

    ; Far jump with CORRECT hardcoded address
    db 0xEA           ; Far jump opcode
    dd 0x20052        ; Absolute address of protected_mode (0x20000 + 0x52)
    dw 0x08           ; Code segment selector

bits 32
protected_mode:
    ; Setup 32-bit segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    ; Write '3' and '2' in 32-bit mode
    mov dword [0xB800C], 0x07320733  ; '32' at position 6

.hang:
    jmp .hang

; GDT  
align 8
gdt_start:
    dq 0x0000000000000000    ; Null descriptor
gdt_code: 
    dq 0x00CF9A000000FFFF    ; 32-bit code segment
gdt_data:
    dq 0x00CF92000000FFFF    ; 32-bit data segment
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd 0x20070   ; Hardcoded GDT absolute address (0x20000 + 0x70)