bits 16

section .text

extern cstart_
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
    or eax, 1                 ; Set PE bit
    mov cr0, eax
    mov word [es:8], 0x074D   ; 'M'

    ; Show we're about to far jump
    mov word [es:10], 0x074A  ; 'J' for Jump

    ; Far jump to switch to 32-bit mode
    jmp CODE_SEG:protected_mode

    ; If we reach here, the far jump FAILED
    mov word [es:12], 0x0746  ; 'F' for Failed
    jmp hang16

bits 32
protected_mode:
    cli
    ; Setup 32-bit segments FIRST
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Write to VGA to confirm we're in 32-bit mode
    mov dword [0xB800C], 0x07580758   ; 'XX'
    
    ; Setup 32-bit stack
    mov esp, 0x90000
    mov ebp, esp

    ; Show stack is set up
    mov dword [0xB80010], 0x075A075A  ; 'ZZ'

    ; Call C code
    push 0x80
    call cstart_
    add esp, 4

hang32:
    cli
    hlt
    jmp hang32

bits 16
hang16:
    cli
    hlt
    jmp hang16

; === Data section ===
boot_drive: db 0

; === GDT ===
align 8
gdt_start:
    dq 0x0000000000000000    ; Null descriptor
gdt_code: 
    dq 0x00CF9A002000FFFF    ; 32-bit code segment, base = 0x20000
gdt_data:
    dq 0x00CF92002000FFFF    ; 32-bit data segment, base = 0x20000
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start             ; Use the actual address of gdt_start

; Segment selectors  
CODE_SEG equ gdt_code - gdt_start  ; 0x08
DATA_SEG equ gdt_data - gdt_start  ; 0x10